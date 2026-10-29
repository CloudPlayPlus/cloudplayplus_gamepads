import Foundation
#if CPP_LOG_AVAILABLE
import Darwin

private typealias CppLogEmit = @convention(c) (Int32, UnsafePointer<CChar>, UnsafePointer<CChar>) -> Void
private let cppLogEmit: CppLogEmit? = {
    // iOS 静态链接从进程查找；macOS 与产品现有 Swift 日志桥一致。
    let paths: [String?] = [nil, "@rpath/cpp_log.framework/cpp_log"]
    for path in paths {
        if let handle = dlopen(path, RTLD_LAZY | RTLD_LOCAL) {
            if let symbol = dlsym(handle, "cpp_log_emit") {
                return unsafeBitCast(symbol, to: CppLogEmit.self)
            }
            dlclose(handle)
        }
    }
    return nil
}()
#endif

final class RumbleDiagnostics {
    private let lock = NSLock()
    private var last: [String: TimeInterval] = [:]
    func record(_ event: String, error: Error?) {
#if CPP_LOG_AVAILABLE
        guard let emit = cppLogEmit else { return }
        let now = ProcessInfo.processInfo.systemUptime
        lock.lock()
        let allowed = last[event].map { now - $0 >= 5 } ?? true
        if allowed { last[event] = now }
        lock.unlock()
        guard allowed else { return }
        let diagnostic = error as NSError?
        let detail = diagnostic.map { " domain=\($0.domain) code=\($0.code)" } ?? ""
        "GAMEPAD_RUMBLE".withCString { tag in
            (event + detail).withCString { emit(error == nil ? 2 : 3, tag, $0) }
        }
#endif
    }
}
