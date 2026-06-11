import pygame
import serial
import time

# ================ CONFIGURATION =================
SERIAL_PORT = 'COM3'
BAUD_RATE = 115200
# ==================================================

def map_speed(val):
    """Maps joystick float (-1.0 to 1.0) tp PWM int (-255 to 255)"""
    # Deadzone: Ignore small drifts < 10%
    if abs(val) < 0.1:
        return 0
    return int(val * 255)

def main():
    # 1. Setup Controller
    pygame.init()
    pygame.joystick.init()

    if pygame.joystick.get_count() == 0:
        print("❌ No Game Controller found! Check your USB dongle.")
        return
    
    joystick = pygame.joystick.Joystick(0)
    joystick.init()
    print(f"🎮 Controller Connected: {joystick.get_name()}")

    #2. Setup Serial Connection
    try:
        esp32 = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout = 0.1)
        time.sleep(2) # Allow ESP32 to reset
        print(f"🔌 Connected to ESP32 on {SERIAL_PORT}")
    except Exception as e:
        print(f"❌ Serial Error: {e}")
        print("Tip: Check if the Arduino  IDE Serial Monitor is open. Close it!")
        return
    
    print("\n--- CONTROLS (Tank Drive) ---")
    print("Left Stick Fwd/Back  : Left Wheel")
    print("Right Stick Fwd/Back : Right Wheel")
    print("Button A / X         : Seed Meter (Hold to spin)")
    print("--- LOGIC ---")

    try:
        while True:
            pygame.event.pump()

            # --- INPUTS ---
            left_stick = -joystick.get_axis(1)
            right_stick = joystick.get_axis(4)
            seed_button = joystick.get_button(0)

            # --- LOGIC ---
            l_pwm = map_speed(left_stick)
            r_pwm = map_speed(right_stick)

            # --- REAR MOTOR LOGIC ---
            turn_intensity = abs(l_pwm - r_pwm)
            
            # If driving roughly straight (difference is small), kill rear motor
            if turn_intensity < 40: 
                b_pwm = 0
            else:
                # If turning, set rear motor to the average forward speed
                b_pwm = int((l_pwm + r_pwm) / 2)

            # --- SEED METER LOGIC ---
            s_pwm = 200 if seed_button else 0

            # --- SEND DATA ---
            # Format: <L,R,B,S>
            command = f"<{l_pwm},{r_pwm},{b_pwm},{s_pwm}>\n"
            esp32.write(command.encode())
            
            # Debug Print
            status = "STRAIGHT" if b_pwm == 0 else "TURNING "
            print(f"[{status}] L:{l_pwm} R:{r_pwm} | Rear:{b_pwm} | Seed:{s_pwm}   ", end='\r')
            
            time.sleep(0.05) 

    except KeyboardInterrupt:
        print("\n🛑 Stopping...")
        esp32.write(b"<0,0,0,0>\n")
        esp32.close()

if __name__ == "__main__":
    main()
