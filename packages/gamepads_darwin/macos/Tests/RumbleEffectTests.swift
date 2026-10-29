import Foundation

private enum Failure: Error { case injected }

private final class Player: RumblePlayer {
    var amplitudes: [Double] = []
    var starts = 0
    var stops = 0
    var cancels = 0
    var failUpdate = false
    var failStop = false
    func setIntensity(_ value: Double) throws {
        if failUpdate { throw Failure.injected }
        amplitudes.append(value)
    }
    func start() throws { starts += 1 }
    func stop() throws {
        stops += 1
        if failStop { throw Failure.injected }
    }
    func cancel() throws { cancels += 1 }
}

private final class Engine: RumbleEngine {
    var player = Player()
    var created = 0
    var stops = 0
    func makePlayer() throws -> RumblePlayer {
        created += 1
        return player
    }
    func start() throws {}
    func shutdown() { stops += 1 }
}

@main
struct RumbleEffectTests {
    static func main() {
        let left = Engine(), right = Engine()
        let effect = RumbleEffect(engines: [left, right], log: { _, _ in })
        precondition(effect.update([0.4, 0.6], duration: 10, now: 0))
        precondition(effect.update([0.2, 0.8], duration: 10, now: 4))
        precondition(left.created == 1 && right.created == 1)
        precondition(left.player.starts == 1 && left.player.stops == 0)
        precondition(left.player.amplitudes == [0.4, 0.2])
        // 旧截止时间不能结束更新后的效果；相同强度同样续期。
        effect.expire(now: 10)
        precondition(left.player.stops == 0)
        precondition(effect.update([0.2, 0.8], duration: 10, now: 11))
        effect.expire(now: 14)
        precondition(left.player.stops == 0)
        effect.expire(now: 21)
        precondition(left.player.stops == 1 && right.player.stops == 1)
        precondition(effect.deadline == nil)
        // 到期后复用播放器；单路归零不重启或停止另一马达。
        precondition(effect.update([1, 1], duration: 10, now: 22))
        precondition(left.created == 1 && left.player.starts == 2)
        precondition(effect.update([0, 0.5], duration: 10, now: 23))
        precondition(left.player.stops == 2 && right.player.stops == 1)
        precondition(right.player.starts == 2)
        precondition(effect.stop(reason: "zero"))
        precondition(right.player.stops == 2 && effect.deadline == nil)
        effect.dispose(reason: "disconnect")
        precondition(left.player.cancels == 1 && left.stops == 1)
        precondition(right.player.cancels == 1 && right.stops == 1)
        precondition(!effect.update([1, 1], duration: 10, now: 24))
        effect.dispose(reason: "late callback")
        precondition(left.stops == 1)

        // 停振失败仍须取消所有播放器并关闭所有引擎。
        let bad = Engine(), other = Engine()
        var errors = 0
        let failed = RumbleEffect(engines: [bad, other], log: { _, error in
            if error != nil { errors += 1 }
        })
        precondition(failed.update([1, 1], duration: 10, now: 0))
        bad.player.failStop = true
        precondition(!failed.stop(reason: "zero"))
        precondition(!failed.isValid && failed.deadline == nil)
        precondition(bad.player.cancels == 1 && other.player.cancels == 1)
        precondition(bad.stops == 1 && other.stops == 1 && errors > 0)

        // 更新失败不能留下另一个马达继续震动。
        let first = Engine(), second = Engine()
        let partial = RumbleEffect(engines: [first, second], log: { _, _ in })
        precondition(partial.update([1, 1], duration: 10, now: 0))
        second.player.failUpdate = true
        precondition(!partial.update([0.5, 0.5], duration: 10, now: 1))
        precondition(!partial.isValid && first.stops == 1 && second.stops == 1)

        // reset/stopped 和后台清理共用 dispose，不重放旧反馈。
        let oldEngine = Engine(), newEngine = Engine()
        let old = RumbleEffect(engines: [oldEngine], log: { _, _ in })
        precondition(old.update([1], duration: 10, now: 0))
        old.dispose(reason: "reset")
        let fresh = RumbleEffect(engines: [newEngine], log: { _, _ in })
        precondition(fresh.update([0.5], duration: 10, now: 1))
        old.dispose(reason: "late stopped")
        precondition(fresh.isValid && newEngine.stops == 0)
        fresh.dispose(reason: "background")
        precondition(fresh.deadline == nil && newEngine.stops == 1)
        print("RumbleEffect regressions passed")
    }
}
