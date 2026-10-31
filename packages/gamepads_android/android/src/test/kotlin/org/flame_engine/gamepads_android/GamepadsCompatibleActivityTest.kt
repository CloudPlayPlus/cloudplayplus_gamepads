package org.flame_engine.gamepads_android

import android.hardware.input.InputManager
import android.os.Handler
import android.view.InputDevice
import android.view.KeyEvent
import android.view.MotionEvent
import org.junit.jupiter.api.Assertions.assertFalse
import org.junit.jupiter.api.Assertions.assertTrue
import org.junit.jupiter.api.Test
import org.mockito.Mockito.mock
import org.mockito.Mockito.`when`

class GamepadsCompatibleActivityTest {
    private val activity = object : GamepadsCompatibleActivity {
        override fun registerInputDeviceListener(listener: InputManager.InputDeviceListener, handler: Handler?) {}
        override fun registerKeyEventHandler(handler: (KeyEvent) -> Boolean) {}
        override fun registerMotionEventHandler(handler: (MotionEvent) -> Boolean) {}
    }

    private fun device(
        name: String = "Wireless Controller",
        sources: Int = InputDevice.SOURCE_GAMEPAD or InputDevice.SOURCE_JOYSTICK,
        keyboardType: Int = InputDevice.KEYBOARD_TYPE_NONE,
        axisSource: Int? = InputDevice.SOURCE_JOYSTICK,
        axis: Int = MotionEvent.AXIS_X,
    ): InputDevice {
        val device = mock(InputDevice::class.java)
        `when`(device.name).thenReturn(name)
        `when`(device.sources).thenReturn(sources)
        `when`(device.keyboardType).thenReturn(keyboardType)
        val ranges = if (axisSource == null) emptyList() else {
            val range = mock(InputDevice.MotionRange::class.java)
            `when`(range.source).thenReturn(axisSource)
            `when`(range.axis).thenReturn(axis)
            listOf(range)
        }
        `when`(device.motionRanges).thenReturn(ranges)
        return device
    }

    @Test fun fingerprintScannerIsRejectedEvenWhenItClaimsJoystickAxes() {
        assertFalse(activity.isGamepadsInputDevice(device(name = "uinput-fpc")))
    }

    @Test fun existingFalsePositiveDevicesStayBlocked() {
        for (name in listOf("uinput-goodix", "uinput-xiaomi", "Xiaomi Consumer")) {
            assertFalse(activity.isGamepadsInputDevice(device(name = name)), name)
        }
    }

    @Test fun alphabeticKeyboardIsRejectedEvenWithJoystickAxes() {
        for (sources in listOf(InputDevice.SOURCE_JOYSTICK,
            InputDevice.SOURCE_GAMEPAD or InputDevice.SOURCE_JOYSTICK)) {
            assertFalse(activity.isGamepadsInputDevice(device(
                sources = sources,
                keyboardType = InputDevice.KEYBOARD_TYPE_ALPHABETIC,
            )))
        }
    }

    @Test fun vxeCompositeKeyboardWithGenericJoystickAxisIsRejected() {
        assertFalse(activity.isGamepadsInputDevice(device(
            name = "VXE V87 BT2 Keyboard",
            sources = InputDevice.SOURCE_KEYBOARD or InputDevice.SOURCE_DPAD or
                InputDevice.SOURCE_MOUSE_RELATIVE or InputDevice.SOURCE_JOYSTICK,
            keyboardType = InputDevice.KEYBOARD_TYPE_ALPHABETIC,
            axis = MotionEvent.AXIS_GENERIC_1,
        )))
    }

    @Test fun ordinaryJoystickIsStillAccepted() {
        assertTrue(activity.isGamepadsInputDevice(device()))
    }

    @Test fun alphabeticKeyboardClaimingGamepadSourceIsRejectedWithoutJoystickAxes() {
        for (axisSource in listOf(null, InputDevice.SOURCE_MOUSE)) {
            assertFalse(activity.isGamepadsInputDevice(device(
                name = "Bluetooth Keyboard",
                keyboardType = InputDevice.KEYBOARD_TYPE_ALPHABETIC,
                axisSource = axisSource,
            )))
        }
    }

    @Test fun keyboardWithoutGamepadSourceIsRejected() {
        assertFalse(activity.isGamepadsInputDevice(device(
            sources = InputDevice.SOURCE_KEYBOARD,
            keyboardType = InputDevice.KEYBOARD_TYPE_ALPHABETIC,
            axisSource = null,
        )))
    }

    @Test fun buttonOnlyGamepadIsStillAccepted() {
        assertTrue(activity.isGamepadsInputDevice(device(
            sources = InputDevice.SOURCE_GAMEPAD,
            axisSource = null,
        )))
    }

    @Test fun unrelatedUinputControllerIsNotBlockedByPrefix() {
        assertTrue(activity.isGamepadsInputDevice(device(name = "uinput-controller")))
    }
}
