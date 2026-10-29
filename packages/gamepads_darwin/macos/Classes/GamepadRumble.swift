import Foundation
import GameController
import CoreHaptics
#if os(iOS)
import UIKit
#else
import AppKit
#endif

@available(macOS 11.0, iOS 14.0, *)
private final class AppleRumblePlayer: RumblePlayer {
    let player: CHHapticPatternPlayer
    init(_ player: CHHapticPatternPlayer) { self.player = player }
    func setIntensity(_ value: Double) throws {
        try player.sendParameters([
            CHHapticDynamicParameter(parameterID: .hapticIntensityControl,
                                     value: Float(value), relativeTime: 0)
        ], atTime: CHHapticTimeImmediate)
    }
    func start() throws { try player.start(atTime: CHHapticTimeImmediate) }
    func stop() throws { try player.stop(atTime: CHHapticTimeImmediate) }
    func cancel() throws { try player.cancel() }
}

@available(macOS 11.0, iOS 14.0, *)
private final class AppleRumbleEngine: RumbleEngine {
    let engine: CHHapticEngine
    let log: (String, Error?) -> Void
    init(_ engine: CHHapticEngine, log: @escaping (String, Error?) -> Void) {
        self.engine = engine
        self.log = log
        // 普通停振保留播放器，避免 idle shutdown 使引用失效。
        engine.isAutoShutdownEnabled = false
    }
    func start() throws { try engine.start() }
    func makePlayer() throws -> RumblePlayer {
        // 动态强度乘以基础强度，因此基础强度必须为 1。
        let event = CHHapticEvent(eventType: .hapticContinuous, parameters: [
            CHHapticEventParameter(parameterID: .hapticIntensity, value: 1)
        ], relativeTime: 0, duration: TimeInterval(GCHapticDurationInfinite))
        return AppleRumblePlayer(try engine.makePlayer(
            with: CHHapticPattern(events: [event], parameters: [])))
    }
    func shutdown() {
        engine.stoppedHandler = nil
        engine.resetHandler = nil
        engine.stop { [log] error in
            if let error = error { log("engine_stop_failed", error) }
        }
    }
}

@available(macOS 11.0, iOS 14.0, *)
final class GamepadRumble {
    private let queue = DispatchQueue(label: "gamepads.rumble")
    private let queueKey = DispatchSpecificKey<Bool>()
    private var effects: [ObjectIdentifier: RumbleEffect] = [:]
    private var timer: DispatchSourceTimer?
    private var observers: [NSObjectProtocol] = []
    private var suspended = false
    private let diagnostics = RumbleDiagnostics()

    init() {
        queue.setSpecific(key: queueKey, value: true)
        let timer = DispatchSource.makeTimerSource(queue: queue)
        timer.setEventHandler { [weak self] in self?.expire() }
        timer.schedule(deadline: .distantFuture)
        timer.resume()
        self.timer = timer
        observe(.GCControllerDidDisconnect) { [weak self] notification in
            guard let self = self, let controller = notification.object as? GCController else { return }
            self.queue.sync {
                self.effects.removeValue(forKey: ObjectIdentifier(controller))?.dispose(reason: "disconnect")
                self.scheduleExpiry()
            }
        }
#if os(iOS)
        observe(UIApplication.didEnterBackgroundNotification) { [weak self] _ in self?.setSuspended(true) }
        observe(UIApplication.willEnterForegroundNotification) { [weak self] _ in self?.setSuspended(false) }
        suspended = UIApplication.shared.applicationState == .background
#else
        observe(NSApplication.didResignActiveNotification) { [weak self] _ in self?.setSuspended(true) }
        observe(NSApplication.didBecomeActiveNotification) { [weak self] _ in self?.setSuspended(false) }
#endif
    }
    deinit {
        for observer in observers { NotificationCenter.default.removeObserver(observer) }
        timer?.cancel()
        // 等待已排队的引擎回调；最后引用也可能在计时回调中释放，避免同队列 sync。
        let cleanup = { [effects] in
            for effect in effects.values { effect.dispose(reason: "destroy") }
        }
        if DispatchQueue.getSpecific(key: queueKey) == true { cleanup() }
        else { queue.sync(execute: cleanup) }
    }
    private func observe(_ name: Notification.Name, _ block: @escaping (Notification) -> Void) {
        observers.append(NotificationCenter.default.addObserver(forName: name, object: nil,
                                                                queue: .main, using: block))
    }
    private func setSuspended(_ value: Bool) {
        queue.sync {
            suspended = value
            if value { clear(reason: "background") }
        }
    }
    private func localities(_ controller: GCController) -> [GCHapticsLocality] {
        guard let supported = controller.haptics?.supportedLocalities else { return [] }
        if supported.contains(.leftHandle) && supported.contains(.rightHandle) {
            return [.leftHandle, .rightHandle]
        }
        let candidates: [GCHapticsLocality] = [.default, .handles, .leftHandle, .rightHandle]
        for locality in candidates {
            if supported.contains(locality) { return [locality] }
        }
        return []
    }
    func has(_ controller: GCController) -> Bool { !localities(controller).isEmpty }
    func set(_ controller: GCController, low: Double, high: Double, duration: Int) -> Bool {
        guard low.isFinite, high.isFinite, (0...1).contains(low), (0...1).contains(high),
              (0...30000).contains(duration) else { return false }
        if duration == 0 || (low == 0 && high == 0) { return stop(controller) }
        return queue.sync {
            guard !suspended else { return false }
            let key = ObjectIdentifier(controller)
            let sites = localities(controller)
            guard !sites.isEmpty else { return false }
            if effects[key]?.isValid != true { effects[key] = makeEffect(controller, sites: sites) }
            guard let effect = effects[key] else { return false }
            let result = effect.update(sites.count == 2 ? [low, high] : [max(low, high)],
                                       duration: Double(duration) / 1000,
                                       now: ProcessInfo.processInfo.systemUptime)
            if !effect.isValid { effects.removeValue(forKey: key) }
            scheduleExpiry()
            return result
        }
    }
    private func makeEffect(_ controller: GCController, sites: [GCHapticsLocality]) -> RumbleEffect? {
        guard let haptics = controller.haptics else { return nil }
        let diagnostics = self.diagnostics
        let log: (String, Error?) -> Void = { event, error in diagnostics.record(event, error: error) }
        var engines: [AppleRumbleEngine] = []
        for locality in sites {
            guard let engine = haptics.createEngine(withLocality: locality) else {
                log("engine_create_failed", nil)
                for engine in engines { engine.shutdown() }
                return nil
            }
            engines.append(AppleRumbleEngine(engine, log: log))
        }
        let effect = RumbleEffect(engines: engines, log: log)
        let queue = self.queue
        for engine in engines {
            engine.engine.stoppedHandler = { [weak effect] reason in
                queue.async { [weak effect] in
                    guard let effect = effect, effect.isValid else { return }
                    log("engine_stopped_\(reason.rawValue)", nil)
                    effect.dispose(reason: "engine_stopped")
                }
            }
            engine.engine.resetHandler = { [weak effect] in
                queue.async { [weak effect] in
                    guard let effect = effect, effect.isValid else { return }
                    effect.dispose(reason: "engine_reset")
                }
            }
        }
        return effect
    }
    @discardableResult
    func stop(_ controller: GCController) -> Bool {
        queue.sync {
            let result = effects[ObjectIdentifier(controller)]?.stop(reason: "zero") ?? true
            scheduleExpiry()
            return result
        }
    }
    func stopAll() { queue.sync { clear(reason: "stop_all") } }
    private func clear(reason: String) {
        for effect in effects.values { effect.dispose(reason: reason) }
        effects.removeAll()
        scheduleExpiry()
    }
    private func expire() {
        let now = ProcessInfo.processInfo.systemUptime
        for effect in effects.values { effect.expire(now: now) }
        scheduleExpiry()
    }
    private func scheduleExpiry() {
        if let deadline = effects.values.compactMap({ $0.deadline }).min() {
            timer?.schedule(deadline: .now() + max(0, deadline - ProcessInfo.processInfo.systemUptime))
        } else { timer?.schedule(deadline: .distantFuture) }
    }
}
