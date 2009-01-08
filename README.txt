SSEBR For Windows XP version 2.0
Sad Samsung CLP-510 EEPROM Backup/Restore utility
Based on the original idea of SSEBR for DOS

Usage: ssebr2.exe [options]

 -h             = this help screen
 -w             = print wiring information
 -p <port>      = set LPT port (1, 2 or 3)
 -i             = view chip information
 -b <file name> = backup EEPROM
 -r <file name> = restore EEPROM
 -z             = zero out page counter
 -n             = don't save backup
 -f             = force incompatible write


How to connect the cartridge to the PC?

Connect to an LPT port using 3 wires as described below.

25-pin LPT connector at the PC:
        -----------------------------
     13 \ x x x x x x x x x x x x x / 1
      25 \ x x x x x x x x x x x x / 14
           -----------------------

Cartridge connector:
 +--------------------------------------------------------+
 |       / \                                 .            |
 |       \ /               +---------------+            O +
 |                         | [D]  [G]  [C] |             /
 +------------------------------------------------------+

        D = Data    <connect-to>  LPT pin 16
        G = Ground  <connect-to>  LPT pin 25
        C = Clock   <connect-to>  LPT pin 17
        
Image transfer belt:
 |                                                         |
++---------------------------------------------------------+
                 |        DCG             |
                 |     | (+++++++) |      |
                 |     \  +++++++  |      |
                 +------------------------+


LPT port must support bidirectional communication and must not be low-power.


This application is provided on AS-IS basis without any warranty.
