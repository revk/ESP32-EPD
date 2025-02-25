# General E-paper signage

## Trademark

This is an open source project, but bear in mind you cannot sell boards bearing the Andrews & Arnold Ltd name, the A&A logo, the registered trademark AJK logo, or the GS1 allocated EANs assigned to Andrews & Arnold Ltd.

# ESP32-EPD

This used the ESP32-GFX library and provides a general E-paper sign with a configurable selection of *widgets*.

This is the nmew version of my `EPDSign` code which had a few options (clock, date, day, wifi and QR, backgroudn images, etc).

This new version is much more generic, allowing a number of *widgets* to be applied to the displayed images one after the other, this allowing the overall image to be constructed to suit any need.

## Widgets

Each widget has basic settings with *type*, and position (*x*/*y*), and alignment (*left*/*centre*/*right* and *top*/*middle*/*bottom*) and the *size* of the widget, and, of course a *content* setting.

### Text

This is one of the simplest widgets, and allows simple mulkti line text to be displayed. The font size defines the text size (-ve value means allow for descenders).

The *content* for any widget can be one of a number of presets, as follows :-

|Preset|<eaning|
|------|-------|
|`$TIME`|Current time `HH:MM`|
|`$DATE`|Current date `YYYY-MM-DD`|
|`$DAY`|Current day, e.g. `WEDNESDAY`|
|`$SSID`|Current WiFi SSID|
|`$PASS`|Current WiFi passphrase|
|`$WIFI`|QR code formatted current WiFi details|
|`$IPV6`|Current main IPv6 (also `$IP`)|
|`$IPV4`|Current IPv4|

More may be added over time. All of these are only for whole string replacing it.

### Blocks

Same as `text` but blocky characters, so can allow larger size than defined fonts in build.

### Digits

Same as `text` but using 7 seg format, allows quite large digits typicallty. This is ideal for `$DATE` and `$TIME`. It plots only the segments as black or white (see *mask* below).

### Image

The content should be a simple `http://` URL serving a PNG image. It is recommended that this is 1 bit indexed, but can be any valid PNG (memory permitting). It can include *alpha* channel to control if plotted.

The image is typcially stored in SD card if present as a backup.

Other formats for SD card only are planed.

### QR

The content is content of QR code, and size if the overall size (width and height) in pixels.

### HLine/VLine

Draws a horizontal or virtial line based on size and alignment.

### More

More widgets planned

## Align, mask, and invert

For most widgets the *x*/*y* and *align* apply as expected. But there is also a mask/invert setting.

Invert simply swaps black/white.

Mask means only plot for asserted colour - content of text or white in am image. However, when dealing with an *image* the mask is not sensible when the PNG image has alpha as that is used as mask.