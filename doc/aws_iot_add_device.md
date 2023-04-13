# Adding a Device to AWS IoT

## Pre-requisites

- a LoRaWAN device powered by the NM180100 such as the NM180100EVB.
- the NM180100 device is running the LoRaWAN protocol stack provided in NMSDK2.

## Profile Configuration

Device and service profiles define common device configurations and are shared
by multiple devices. Multiple profiles can exist to support different devices.

### Device Profile Creation

1. Login to AWS and type `IoT Core` in the search bar at the top.
2. Select **IoT Core** in the search result to access the AWS IoT console.
4. Select **LPWAN devices** in the left panel to expand the menu.
5. Select **Profiles** in the submenu.
6. Click on **Add device profile**.
7. In the **_Select a default profile and customize_** drop down box, select a default profile as a starting point. In this example, we will be using US915-A for the 900MHz ISM Band Class A operation in North America.
8. Enter a profile name. In this example, we will use `demo_us915_a_otaa`.
9. Select US915 in **Frequency band**.
10. Select 1.1 in **MAC version**.
11. Leave the **Regional parameters version** to the default. RP002-1.0.1 at the time of this writing.
12. Change MaxEIRP to 15. This parameter refers to the maximum radiated power which is the sum of the conducted power and the maximum antenna gain. For the NM180100, the conducted power is around 21.5dBm at the antenna pin. If the maximum antenna gain is 2dBi, then the EIRP of this device is 23.5dBm. However, AWS restricts the entry to less than 16. This does not impact device performance and is used by the link control algorithm to determine the optimal modulation parameters.
13. Enable additional classes if your application supports them.  While the NMSDK2 supports all three classes, Class B operation requires support from the gateway manufacturer as a Class B gateway requires a GPS and the concentrator chip must be provided with a GPS derived pps clock.  In this example, we will leave both Class B and C disabled.
14. Ensure **Supports Join** is enabled.
15. Ensure that **Supports 32-bit FCnt** is enabled.
16. Click **Add device profile** to finish.

### Service Profile Creation
1. In the IoT console, select **LPWAN devices** on the left panel to expand the menu.
2. Select **Profiles** in the submenu.
3. Click on **Add service profile**.
4. Enter a name for this service profile.  In this example, we will use `demo_service_profile`.
5. Leave **AddGWMetaData** enabled.
6. Click **Add service profile** to save and exit.

## Destination Configuration

## Add a LoRaWAN Device to AWS IoT Core
