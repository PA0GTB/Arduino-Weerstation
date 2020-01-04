# Arduino-Weerstation
Arduino Weerstation met een NodeMCU V2 of V3 board.

Een Regionaal opererend weerstation opgebouwd rondom een NodeMCU board en een 3.2 Inch TFT Display.
Periodiek wordt de actuele weerinformatie bij een zelf gekozen regionaal KNMI weerstation opgehaald en getoond op het Display
Tevens wordt het actuele regionaal geldende weeralarm opgehaald en getoond via kleuren leds.
Via Buienradar wordt de eventuele regeninformatie gedownload en getoond in milimeters per uur (mmu)
In het geval van vorst, wordt dit via een aparte witte Led aangegeven

In de bijlagen zijn de benamingen en codes van de officiele KNMI stations vermeld alsmede de kaartcoordinaten
In de bijlagen bevinden zich ook het schema en de onderdelenlijst

Er wordt gebruik gemaakt van de TFT_eSPI library

sketch geupdated in novermber 2019 ivm enkele typo's
