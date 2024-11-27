M5Stack M5Dial Timezones data to Google

This is an extension of my [repo1](https://github.com/PaulskPt/M5Stack_Atom_Matrix_Timezones),
[repo2](https://github.com/PaulskPt/M5Stack_M5Atom_EchoSPKR) and
[repo3](https://github.com/PaulskPt/M5Dial_Timezones_and_beep_cmd_to_M5AtomEcho)

Important credit:

I was only able to create and successfully finish this project with the great help of Microsoft AI assistant CoPilot.
CoPilot helped me correcting wrong C++ code fragments. It suggested C++ code fragments. CoPilot gave me, in one Q&A session, a "workaround" 
for certain limitation that the Arduino IDE has with the standard C++ specification. And CoPilot gave it's answers at great speed of response.
There were no delays. The answers were instantaneous! Thank you CoPilot. Thank you Microsoft for this exciting new tool!

Hardware used:

    1) M5Stack M5Dial;
    2) M5Stack M5Atom Echo;
    3) GROVE 4-wire cable;

The software consists of two parts: a) for the M5Dial; b) for the M5Atom Echo.

The M5Dial part:
Using RFID TAG recognition. Now you can put the display asleep or awake with your RFID TAG.
The ID number of the RFID TAG has to be copied into the file ```secret.h```, variable: ```SECRET_MY_RFID_TAG_NR_HEX```,
for example: ```2b8e3942``` (letters in lower case). A global variable ```use_rfid``` (default: ```true```), controls if RFID recognition
will be active or not (in that case display touch will be the way to put the display asleep or awake).

After applying power to the M5Dial device, the sketch will sequentially display data of seven pre-programmed timezones.

For each of the seven timezones, in four steps, the following data will be displayed:
   1) Time zone continent and city, for example: "Europe" and "Lisbon"; 
   2) the word "Zone" and the Timezone in letters, for example "CEST", and the offset to UTC, for example "+0100";
   3) date info, for example "Monday September 30 2024"; 
   4) time info, for example: "20:52:28 in: Lisbon".

Each time zone sequence of four displays is repeated for 25 seconds. This repeat time is defined in function ```loop()```:

```
901 unsigned long const zone_chg_interval_t = 25 * 1000L; // 25 seconds
```

Data to Google:

New in this version is that I added the functionality to send some data to a Google Sheets Spreadsheet.
The following data will be sent: 
```
1) a datetime stamp in GMT time;
2) SNTP sync time (in epoch value);
3) difference time, in seconds, between the last SNTP sync time (epoch) moment and the current;
4) an index number, ranging from 0 to 23;
5) an integer value indicating the state of the display: "1" = display on, "0" = display off;
6) value of FreeHeap memory in bytes;
7) name of the device that sent the data, in our case "M5Dial".
```
To acomplish this, the data is first sent through a ```HTML POST``` request to a Google Apps Scripts script which analyses the data and then adds the data to the Google Sheets spreadsheet. Average it takes 5 seconds between the moment the sketch sends the HTTP POST request and the moment that the data sent appears in the spreadsheet.

Prerequesits:
To be able to successfully send data to a Google Sheets spreadsheet directly (not through a "man-in-the-middle" service like "Pushingbox") one needs to have:
- a Google account;
- a Google Cloud account; (note: that until this moment I did not have to pay Google any fees for these services).
- a Google Cloud project created. The name of my project is: ```My data archive```;
- in Google Cloud create service account(s) and API key(s). The API key one needs to copy to the file ```secret.h```, variable: ```SECRET_GOOGLE_API_KEY```.
- in Google Apps Scripts, create a Script. Mine I named: ```M5Dial_Timezones```. Inside this development environment I created a script with the name: ```Code.gs```. For this script I created a ```deployment```. Every time you make changes to this script, you need to create a ```new deployment```. Old deployments one can put to "archive". Then, by clicking in the right up part of the screen on the blue button "Deployment", one creates a new deployment. Next one needs to copy the URL of the ```Web App```. The complete link (starting with "https" and ending with "/exec") one needs to copy to the file ```secret.h``` into the variable: ```SECRET_GOOGLE_APPS_SCRIPT_URL```. In the page "manage deployments" do not forget to select "ME (\<your GMail e-mail address\>)". Also select the next item: access range, select: ```Anyone```. The Web app link has the following format:
  ```https://script.google.com/macros/s/<ID_of_your_deployment_ID>/exec```
- in your Google Drive a spreadsheet. The name I used for this spreadsheet is: ```M5Dial_Timezones_SNTP_sync_times```.

Note that it is not easy to perform the necessary steps in Google Cloud, to create a API service account; to create an API Key and set restrictions for this key.
It took me a while to get everything right, however Google has excellent documentation. As far as I remember for this project we don't need to through OAUTH 2 identification.

At start of the sketch or at a ```software reset``` the variables in ```secret.h```will be copied into the sketch.

M5Dial Display

2024-10-11 Added functionality to switch the display off and On by display touches or by the use of an RFID TAG card. The response of the sketch to a display touch to set the display asleep is not as sensitive as is to the opposite: touch to awake the display. The same happens when using the RFID TAG card.

M5Dial sound:

The M5Dial has a built-in speaker, however my experience is that the sound is very weak, even with the volume set maximum (10).
I also experienced that the audibility of the speaker sound depends on the frequency of the tone played. Another thing I noticed is that when using the speaker, the NTP Time Synchronization moment is delayed each time by 2 seconds. When the speaker is not used, there is no delay in the NTP Time Synchronization moment.
For this reason I decided not to use the M5Dial speaker. As an alternative I added a text in the toprow (see below under ```M5Dial Reset:```),
and in this repo, I added functionality to "use" the ability of the M5Atom Echo device to produce nice sounds, also louder than the speaker of the M5Dial device can produce.
The function ```send_cmd_to_AtomEcho()``` is called at the moment of NTP Time Synchronization, however only when the display is awake. When the display is asleep (off), because the user touched the display to put it asleep, for example to have the display asleep during night time, no beep commands will be send to the Atom Echo device. We don't want sounds during the night or other moments of silence.

M5Dial reset:

Pressing the M5Dial button (of the display) will cause a software reset.
A software reset also occurs when the FreeHeap memory sinks below a preset value (in this moment 200,000 bytes).

On reset the Arduino Sketch will try to connect to the WiFi Access Point of your choice (set in secret.h). 
The sketch will connect to a SNTP server of your choice. In this version the sketch uses a ```SNTP polling system```. 
The following define sets the SNTP polling interval time:

```
67 #define CONFIG_LWIP_SNTP_UPDATE_DELAY  5 * 60 * 1000 // = 5 minutes
```

At the moment of a SNTP Time Synchronization, the text "TS" will be shown in the middle of the toprow of the display.
The sketch will also send a digital impulse via GROVE PORT B of the M5Dial, pin 1 (GROVE white wire).
The internal RTC of the M5Dial device will be set to the SNTP datetime stamp with the local time for the current Timezone.
Next the sketch will display time zone name, timezone offset from UTC, date and time of the current Timezone.

In the M5Dial sketch is pre-programmed a map (dictionary), name ```zones_map```. At start, the function ```create_maps()```
will import all the timezone and timezone_code strings from the file ```secret.h``` into the map ```zones_map```, resulting
in the following map:

```
    zones_map[0] = std::make_tuple("America/Kentucky/Louisville", "EST5EDT,M3.2.0,M11.1.0");
    zones_map[1] = std::make_tuple("America/New_York", "EST5EDT,M3.2.0,M11.1.0");
    zones_map[2] = std::make_tuple("America/Sao_Paulo", "<-03>3");
    zones_map[3] = std::make_tuple("Europe/Lisbon","WET0WEST,M3.5.0/1,M10.5.0");
    zones_map[4] = std::make_tuple("Europe/Amsterdam", "CET-1CEST,M3.5.0,M10.5.0/3");
    zones_map[5] = std::make_tuple("Asia/Tokyo", "JST-9");
    zones_map{6] = std::make_tuple("Australia/Sydney", "AEST-10AEDT,M10.1.0,M4.1.0/3");
```

M5Dial Debug output:

Because of memory limitations all of the if (my_debug) {...} blocks were removed.

File secret.h:

Update the file secret.h as far as needed:
```
 a) your WiFi SSID in SECRET_SSID;
 b) your WiFi PASSWORD in SECRET_PASS;
 c) the name of the NTP server of your choice in SECRET_NTP_SERVER_1, for example: 2.pt.pool.ntp.org;
 d) the SECRET_NTP_NR_OF_ZONES as a string, e.g.: "7";
 e) the TIMEZONE and TIMEZONE_CODE texts for each of the zones you want to be displayed.
 f) the SECRET_GOOGLE_APPS_SCRIPT_URL, e.g.: "https://script.google.com/macros/s/AK___etcetera___/exec"; (key: 72 characters);
 g) the SECRET_GOOGLE_API_KEY, e.g.: AI___etcetera___" (39 characters);
 h) the RFID TAG code in variable: SECRET_MY_RFID_TAG_NR_HEX, a hexadecimal value of 8 characters.

 At this moment file secret.h has the following timezones and timezone_codes defined:
    #define SECRET_NTP_TIMEZONE0 "America/Kentucky/Louisville"
    #define SECRET_NTP_TIMEZONE0_CODE "EST5EDT,M3.2.0,M11.1.0"
    #define SECRET_NTP_TIMEZONE1 "America/New_York"
    #define SECRET_NTP_TIMEZONE1_CODE "EST5EDT,M3.2.0,M11.1.0"
    #define SECRET_NTP_TIMEZONE2 "America/Sao_Paulo"
    #define SECRET_NTP_TIMEZONE2_CODE "<-03>3"
    #define SECRET_NTP_TIMEZONE3 "Europe/Lisbon"
    #define SECRET_NTP_TIMEZONE3_CODE "WET0WEST,M3.5.0/1,M10.5.0"
    #define SECRET_NTP_TIMEZONE4 "Europe/Amsterdam"
    #define SECRET_NTP_TIMEZONE4_CODE "CET-1CEST,M3.5.0,M10.5.0/3"
    #define SECRET_NTP_TIMEZONE5 "Asia/Tokyo"
    #define SECRET_NTP_TIMEZONE5_CODE "JST-9"
    #define SECRET_NTP_TIMEZONE6 "Australia/Sydney"
    #define SECRET_NTP_TIMEZONE6_CODE "AEST-10AEDT,M10.1.0,M4.1.0/3"

As you can see, I have the timezones ordered in offset to UTC, however this is not a must.

```

The M5Atom Echo part:

After applying power to the M5 Echo Atom device, the sketch will wait for a "beep command" impulse on the GROVE PORT pin1, sent by the M5Dial device, or a button press of the button on top of the M5Atom Echo device. Upon pressing the button or receiving a "beep command", a double tone sound will be produced and the RGB Led wil be set to GREEN at the start of the beeps. After the beeps have finished, the RGB Led will be set to RED.

The sketch ```M5Atom_EchoSPKR_beep_on_command_M5Dial.ino``` uses two files: ```AntomEchoSPKR.h``` and ```AntomEchoSPKR.cpp```,
which last two files define the ```class ATOMECHOSPKR```, used by the sketch.
The sketch has functionality to make the M5Atom Echo device to "listen" on Pin1 of the GROVE PORT for a digital impulse (read: beep command)
sent by the M5Dial at moment of a NTP Time Synchronization.

Updates:

2025-10-14: Version 2 for M5Dial: I had to delete a lot of ```if (my_debug)``` blocks and use other measures regarding definitions of certain variables containing texts to get rid of a ```memory full``` error while compiling the sketch. After these measures the memory is occupied for 97 percent. The sketch compiles OK.

2024-10-17: Version 2 for M5Dial: in function ```time_sync_notification_cb()``` changed the code a lot to make certain that the function initTime() gets called only once at the moment of a SNTP synchronization.

2024-10-22: created a version M5Dial with RFID however with few messages to the Serial Monitor to reduce the use of memory.

2024-10-23: totally rebuilt function disp_data(), to eliminate memory leaks.

2024-11-17 Added functionality using FreeRTOS semaphore signalization, using boolean flags ```sntp_busy```  and ```handle_requestBusy```, using also a SemaphoreHandle_t named ```mutex```, to control and safeguard the execution of important functions: the SNTP sync time callback function ```time_sync_notification_cb()``` and the function ```handle_request()```. This solved the problem of doubling of data lines and index number skipovers in the Google Sheets spreadsheet.

Copy of the Google Apps Scripts script:

A copy of the Google Apps Script is in subfolder: ```/src/Google_Apps_Scripts```.

Copy of spreadsheet:

I exported to an Microsoft Excel spreadsheet a copy of the Google Sheets Spreadsheet, so one can see the contents and also the formulars that I used.
The copy is in the subfolder ```/src/Google_Sheets```.

Docs:

```
Monitor_output.txt

```

Images: 

Images, mostly edited screenshots, are in the folder: ```images``` and its subfolder: ```Google_Cloud_Service_account_and_API_key```.


Links to product pages of the hardware used:

- M5Stack M5Dial [info](https://docs.m5stack.com/en/core/M5Dial);
- M5Stack M5Atom Echo [info](https://docs.m5stack.com/en/atom/atomecho);
- M5Stack GROVE Cable [info](https://docs.m5stack.com/en/accessory/cable/grove_cable)

If you want a 3D Print design of a stand for the M5Dial, see the post of Cyril Ed on the Printables website:
- [Stand for M5Dial](https://www.printables.com/model/614079-m5stactk-dial-stand).
  I successfully downloaded the files and sent them to a local electronics shop that has a 3D printing service.
  See the images.

Known Issues:

1) Memory leak:

The Arduino sketch has a memory leak. Long time measurements and data analyses showed that during 24 entries in the spreadsheet the average memory loss per entry in the spreadsheet is: 243 bytes. That means that in every function loop() loop, the FreeHeap memory is 243 bytes less. At startup or reset, the value of the FreeHeap is approximately 267756 bytes.
In 24 spreadsheet entries the FreeHeap memory sank until 261816 bytes. This is a loss of 5,940 bytes.
Not only loss of FreeHeap memory occurs, there occurred also moments that there was a "recovery" (gain) of 9,400 bytes!
With help of MS Copilot varios functions of this sketch were investigated for possible memory leakage. Various functions were changed to minimize memory leakage.

2) Display goes black:

It happens that the display of the M5Dial goes black at unpredictable moments. When this happens, long press the display button. This will force a software reset. On November 19, 2024, I created a post on M5Stack Community. See: [post](https://community.m5stack.com/topic/6998/m5dial-display-goes-black-randomly?_=1732462530058). 50 people read the post. Nobody came with a reaction or advice to create a solution. I also searched
M5Stack on Github. I did not find what I was looking for. 
I experienced that after the display went black, the rest of the functionalities of the sketch continued without problem. At times of a SNPTP Time Sync moment I heard the "beep" in the speaker of the M5Echo device. At the same moments I saw online in the Google Sheets spreadsheet a new row with data from the M5Dial being added. When the display went black spontaneously, I experienced that the sketch continued to check for a Button A press and when that button was pressed, the sketch called for a software reset.



