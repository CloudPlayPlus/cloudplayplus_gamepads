package org.flame_engine.gamepads_android

import android.hardware.input.InputManager
import android.os.Handler
import android.view.InputDevice
import android.view.KeyEvent
import android.view.MotionEvent

private val blockedGamepadDeviceNames = setOf(
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
        val isAlphabeticKeyboard =
            device.keyboardType == InputDevice.KEYBOARD_TYPE_ALPHABETIC

        return (hasGamepadSource || hasJoystickSource) && !isAlphabeticKeyboard
    }

    fun registerInputDeviceListener(listener: InputManager.InputDeviceListener, handler: Handler?)
    fun registerKeyEventHandler(handler: (KeyEvent) -> Boolean)
    fun registerMotionEventHandler(handler: (MotionEvent) -> Boolean)
}
