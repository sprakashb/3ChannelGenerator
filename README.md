# 3ChannelGenerator
3channel Clock generator uing Si5351 and TFT touch display
There are two implementations :
1. Using an Arduino Mega with a 3.5 inch MCUFriend TFT with Touch, connected to Si5351.
2. Using an ESP32 with a 2.8 inch SPI touch display and connected to Si5351 module.

More details on vu2spf.blogspot.com  (coming soon)

In both of these the frequency of each channel is adjustable in 1 Hz to 1MHz steps. Any channel can be enabled or disabled. Also there is provision to add correction factor for Si5351, arising due to xtal frequency variations. All channels can be saved on EEPROM/Flash for retrieval at next boot up.

How to use:
Adusting the frequency -
1.Select a channel by touching in the center of that channel's frequency display area.
2. Select step size for changing the frequency from step size selector at the bottom,
3. Touch on the LEFT of frequency display area (between Clkx and Frequency value) to decrease or on the RIGHT of frequency display are ( between frequency value and Hz display) to increase the frequency by step size.
4. Touch SAVE button to save the three frequencies with their selected step sizes for later use.

Calibration-
1. Set a fixed frequency on a channel.
2. Connect a good frequecy counter to that channel's output, note the difference in measured and set values.
3. Touch CAL button, it will open a Calibration display page.
4. Using the correction buttons bring the measured frequency to set fequency as close as possible.
5. Touch back button to go to main screen and save the correction.

Reset to default frequencies-
If need arises the frequency values can be reset to default (which are set in the software).
Touch CAL button, in the calibration page touch RESET button and then Touch YES button. 
Touch Back button to return to main display.
