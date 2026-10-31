package org.flame_engine.gamepads_android

import android.hardware.input.InputManager
import android.os.Handler
import android.view.InputDevice
import android.view.KeyEvent
import android.view.MotionEvent

private val blockedGamepadDeviceNames = setOf(
    // Chromium 同样按名称排除这个被误报为 SOURCE_JOYSTICK 的指纹设备。
    "uinput-fpc",
    "uinput-goodix",
    "uinput-xiaomi",
    "Xiaomi Consumer",
)

interface GamepadsCompatibleActivity {
    fun isGamepadsInputDevice(device: InputDevice): Boolean {
        if (blockedGamepadDeviceNames.contains(device.name.trim())) {
            return false
        }

        val hasGamepadSource =
            device.sources and InputDevice.SOURCE_GAMEPAD == InputDevice.SOURCE_GAMEPAD
        val hasJoystickSource =
            device.sources and InputDevice.SOURCE_JOYSTICK == InputDevice.SOURCE_JOYSTICK
        if (!hasGamepadSource && !hasJoystickSource) {
            return false
        }

        // 复合蓝牙键盘也可能声明 JOYSTICK 轴，不能据此消费它的文字按键。
        val isAlphabeticKeyboard =
            device.keyboardType == InputDevice.KEYBOARD_TYPE_ALPHABETIC

        return !isAlphabeticKeyboard
    }

    fun registerInputDeviceListener(listener: InputManager.InputDeviceListener, handler: Handler?)
    fun registerKeyEventHandler(handler: (KeyEvent) -> Boolean)
    fun registerMotionEventHandler(handler: (MotionEvent) -> Boolean)
}
