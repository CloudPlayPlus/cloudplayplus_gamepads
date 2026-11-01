package org.flame_engine.gamepads_android

import android.view.InputDevice
import android.view.KeyEvent
import android.view.MotionEvent

import io.flutter.plugin.common.MethodChannel
import kotlin.math.abs

data class SupportedAxis(
    val axisId: Int,
    val invert: Boolean = false,
)

class EventListener {
    companion object {
        private const val EPSILON = 0.001
        // 枚举复合设备和转发轴事件使用同一组已支持的轴。
        internal val supportedAxes = listOf(
            SupportedAxis(MotionEvent.AXIS_X),
            SupportedAxis(MotionEvent.AXIS_Y, invert = true),
            SupportedAxis(MotionEvent.AXIS_Z),
            SupportedAxis(MotionEvent.AXIS_RZ, invert = true),
            SupportedAxis(MotionEvent.AXIS_HAT_X),
            SupportedAxis(MotionEvent.AXIS_HAT_Y, invert = true),
            SupportedAxis(MotionEvent.AXIS_LTRIGGER),
            SupportedAxis(MotionEvent.AXIS_RTRIGGER),
            SupportedAxis(MotionEvent.AXIS_BRAKE),
            SupportedAxis(MotionEvent.AXIS_GAS),
            SupportedAxis(MotionEvent.AXIS_WHEEL),
        )
    }
    private val lastAxisValue = mutableMapOf<Int, Float>()

    private fun isCompositeGamepadKey(event: KeyEvent): Boolean {
        // 一些手柄按钮仍以 KEYBOARD 来源上报，按钮键码本身足以判定。
        if (event.keyCode in gamepadButtonKeyCodes) return true
        val hasControllerSource =
            event.source and InputDevice.SOURCE_GAMEPAD == InputDevice.SOURCE_GAMEPAD ||
            event.source and InputDevice.SOURCE_JOYSTICK == InputDevice.SOURCE_JOYSTICK ||
            event.source and InputDevice.SOURCE_DPAD == InputDevice.SOURCE_DPAD
        if (!hasControllerSource) return false
        return when (event.keyCode) {
            KeyEvent.KEYCODE_DPAD_UP, KeyEvent.KEYCODE_DPAD_DOWN,
            KeyEvent.KEYCODE_DPAD_LEFT, KeyEvent.KEYCODE_DPAD_RIGHT,
            KeyEvent.KEYCODE_MEDIA_RECORD -> true
            else -> false
        }
    }

    fun onKeyEvent(keyEvent: KeyEvent, channel: MethodChannel): Boolean {
        val device = InputDevice.getDevice(keyEvent.deviceId)
        // 只收紧复合设备；普通键盘键交回 Activity 原有的键盘路径。
        if (device?.keyboardType == InputDevice.KEYBOARD_TYPE_ALPHABETIC &&
            !isCompositeGamepadKey(keyEvent)) return false
        val arguments = mutableMapOf<String, Any>(
            "gamepadId" to keyEvent.deviceId.toString(),
            "time" to keyEvent.eventTime,
            "type" to "button",
            "key" to KeyEvent.keyCodeToString(keyEvent.keyCode),
            "value" to if (keyEvent.action == KeyEvent.ACTION_DOWN) 1.0 else 0.0,
        )
        if (device != null) {
            arguments["vendorId"] = device.vendorId
            arguments["productId"] = device.productId
        }
        channel.invokeMethod("onGamepadEvent", arguments)
        return true
    }

    fun onMotionEvent(motionEvent: MotionEvent, channel: MethodChannel): Boolean {
        val device = InputDevice.getDevice(motionEvent.deviceId)
        if (device?.keyboardType == InputDevice.KEYBOARD_TYPE_ALPHABETIC &&
            motionEvent.source and InputDevice.SOURCE_JOYSTICK != InputDevice.SOURCE_JOYSTICK) {
            return false
        }
        var handled = false
        supportedAxes.forEach { axis ->
            val axisHandled = reportAxis(motionEvent, channel, axis)
            if (axisHandled) handled = true
        }

        return handled
    }

    private fun reportAxis(
        motionEvent: MotionEvent,
        channel: MethodChannel,
        axis: SupportedAxis,
    ): Boolean {
        val multiplier = if (axis.invert) -1 else 1
        val value = motionEvent.getAxisValue(axis.axisId) * multiplier

        // No-op if threshold is not met
        val lastValue = lastAxisValue[axis.axisId]
        if (lastValue is Float) {
            if (abs(value - lastValue) < EPSILON) {
                return true
            }
        }
        // Update last value
        lastAxisValue[axis.axisId] = value

        val device = InputDevice.getDevice(motionEvent.deviceId)
        val arguments = mutableMapOf<String, Any>(
            "gamepadId" to motionEvent.deviceId.toString(),
            "time" to motionEvent.eventTime,
            "type" to "analog",
            "key" to MotionEvent.axisToString(axis.axisId),
            "value" to value,
        )
        if (device != null) {
            arguments["vendorId"] = device.vendorId
            arguments["productId"] = device.productId
        }
        channel.invokeMethod("onGamepadEvent", arguments)
        return true
    }
}
