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

// 与 Android KeyEvent.isGamepadButton 的按钮范围一致；方向键不能单独证明是手柄。
internal val gamepadButtonKeyCodes = ((KeyEvent.KEYCODE_BUTTON_A..KeyEvent.KEYCODE_BUTTON_MODE) +
    (KeyEvent.KEYCODE_BUTTON_1..KeyEvent.KEYCODE_BUTTON_16)).toIntArray()

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

        // 普通手柄（含纯按键手柄）继续走原来的判断，不增加能力门槛。
        if (device.keyboardType != InputDevice.KEYBOARD_TYPE_ALPHABETIC) return true

        // 只对复合键盘确认实际手柄控件。VXE 等键盘的 GENERIC_1 不能作为摇杆依据。
        val hasSupportedJoystickAxis = device.motionRanges.any { range ->
            range.source and InputDevice.SOURCE_JOYSTICK == InputDevice.SOURCE_JOYSTICK &&
                EventListener.supportedAxes.any { it.axisId == range.axis }
        }
        return hasSupportedJoystickAxis || device.hasKeys(*gamepadButtonKeyCodes).any { it }
    }

    fun registerInputDeviceListener(listener: InputManager.InputDeviceListener, handler: Handler?)
    fun registerKeyEventHandler(handler: (KeyEvent) -> Boolean)
    fun registerMotionEventHandler(handler: (MotionEvent) -> Boolean)
}
