# Digital Dial Indicator Crack Sensor Hardware

[3D-model Onshape Link](https://cad.onshape.com/documents/177fcd6626cff4e7ae1118a5/w/60c44c86a3964531458f62a3/e/0062c1a0376e6109ae3408a2?renderMode=0&uiState=67e3261779fea40c1621292e)

### Materials
- SHAHE Digital Dial Indicator 5307A-10/25 <https://aliexpress.com/item/32663445499.html>
- Heltec Wifi LoRa 32 V3
- Angled Mini-USB-OTG cable or Mini-USB connector with ID-pin
- 100k resistor (2 pcs.)
- 3D printed enclosure (ASA)
- 3D printed brackets (2 pcs.)
- Acrylic glass (2 mm) or 3D printed lid
- Short M3 inserts (10 pcs.)
- M3x6 screws (8 pcs.)
- M3x30 screws (2 pcs.)
- EPDM sealing cord (340 mm)
- Rubber grommet (od 20 mm, id 7 mm)
- Acrylic paint / conformal coating / 3D-print sealant
- Silicone grease

### Tools
- 3D Printer
- Soldering iron
- 3.5 mm drill bit and drill
- Cutter knife

### Assembly
1. Print the enclosure (100% infill, 0.15 mm layer height).
2. Seal the print with spray paint.
3. Print the lid template (0.5 mm).
4. Transfer the template to acrylic glass and cut out the outlines.
5. Drill 3.5 mm screw holes in the lid.
6. Use a soldering iron to melt in the inserts.
7. Lubricate the seal and the grommet with silicone grease and insert them.
8. Cut off the USB-A socket of the OTG-cable and solder the wires to the Heltec microcontroller with the resistors in series as shown in the picture.
9. Insert the SHAHE indicator and secure it with the brackets and M3x30 screws. Use silicone grease to seal the grommet.

<img src="./ShahePinout.drawio.svg" alt="pin out diagram" height="200"/>

## Additional information
- The tip of the shaft of the dial indicator can be unscrewed to extend it with suitable spacers.
- Instead of the USB-OTG-cable wires can be soldered directly to a 5 pin Mini-USB connector.

## Future Improvements / To-Dos / Ideas
- Create a parametric 3D design for the enclosure using FreeCAD.
- Extend enclosure so OTG cable fits in
- Add holes for grommet and inserts for the external temperature sensor