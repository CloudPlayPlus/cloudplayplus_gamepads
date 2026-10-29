import Foundation

// 隔离系统调用，允许无实体手柄验证失败清理及截止时间。
protocol RumblePlayer: AnyObject {
    func setIntensity(_ value: Double) throws
    func start() throws
    func stop() throws
    func cancel() throws
}
protocol RumbleEngine: AnyObject {
    func makePlayer() throws -> RumblePlayer
    func start() throws
    func shutdown()
}

// 所有入口由同一原生串行队列调用。
final class RumbleEffect {
    private struct Motor {
        let engine: RumbleEngine
        var player: RumblePlayer?
        var playing = false
    }
    private var motors: [Motor]
    private let log: (String, Error?) -> Void
    private(set) var deadline: TimeInterval?
    private(set) var isValid = true

    init(engines: [RumbleEngine], log: @escaping (String, Error?) -> Void) {
        motors = engines.map { Motor(engine: $0) }
        self.log = log
    }
    func update(_ amplitudes: [Double], duration: TimeInterval, now: TimeInterval) -> Bool {
        guard isValid, amplitudes.count == motors.count else { return false }
        if duration == 0 || amplitudes.allSatisfy({ $0 == 0 }) { return stop(reason: "zero") }
        do {
            for index in motors.indices {
                if amplitudes[index] == 0 {
                    if motors[index].playing {
                        try motors[index].player?.stop()
                        motors[index].playing = false
                    }
                    continue
                }
                if motors[index].player == nil {
                    try motors[index].engine.start()
                    motors[index].player = try motors[index].engine.makePlayer()
                }
                try motors[index].player?.setIntensity(amplitudes[index])
                if !motors[index].playing {
                    try motors[index].player?.start()
                    motors[index].playing = true
                }
            }
            deadline = now + duration
            return true
        } catch {
            log("update_failed", error)
            dispose(reason: "update_failed")
            return false
        }
    }
    @discardableResult
    func stop(reason: String) -> Bool {
        guard isValid else { return false }
        let wasActive = deadline != nil
        deadline = nil
        do {
            for index in motors.indices where motors[index].playing {
                try motors[index].player?.stop()
                motors[index].playing = false
            }
            if wasActive { log("stop_\(reason)", nil) }
            return true
        } catch {
            log("stop_failed", error)
            dispose(reason: "stop_failed")
            return false
        }
    }
    func expire(now: TimeInterval) {
        if let deadline = deadline, now >= deadline { stop(reason: "timeout") }
    }
    func dispose(reason: String) {
        guard isValid else { return }
        // 先作废，shutdown 的迟到回调不能重入或重放。
        isValid = false
        deadline = nil
        for index in motors.indices {
            do { try motors[index].player?.cancel() }
            catch { log("cancel_failed", error) }
            motors[index].engine.shutdown()
            motors[index].player = nil
            motors[index].playing = false
        }
        log("dispose_\(reason)", nil)
    }
}
