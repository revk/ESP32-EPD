# General E-paper signage

## Trademark

This is an open source project, but bear in mind you cannot sell boards bearing the Andrews & Arnold Ltd name, the A&A logo, the registered trademark AJK logo, or the GS1 allocated EANs assigned to Andrews & Arnold Ltd.

# ESP32-EPD

This used the ESP32-GFX library and provides a general E-paper sign with a configurable selection of *widgets*.

This is the nmew version of my `EPDSign` code which had a few options (clock, date, day, wifi and QR, backgroudn images, etc).

This new version is much more generic, allowing a number of *widgets* to be applied to the displayed images one after the other, this allowing the overall image to be constructed to suit any need.

## Widgets

Each widget has basic settings with *type*, and position (*x*/*y*), and alignment (*left*/*centre*/*right* and *top*/*middle*/*bottom*) and the *size* of the widget, and, of course a *content* setting.

### Content

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

### Text

This is one of the simplest widgets, and allows simple mulkti line text to be displayed. The font size defines the text size (-ve value means allow for descenders).

### Blocks

Same as `text` but blocky characters, so can allow larger size than defined fonts in build.

### Digits

Same as `text` but using 7 seg format, allows quite large digits typicallty. This is ideal for `$DATE` and `$TIME`. It plots only the segments as black or white (see *mask* below). Only handles digits, `-`, `.`, `:`, `_`. Does not handle multiple lines.

### Image

This can be `http://` URL serving a PNG image, or just the end appended to `baseurl`. It is recommended that this is 1 bit indexed, but can be any valid PNG (memory permitting). It can include *alpha* channel to control if plotted.

The image is typcially stored in SD card if present as a backup. If the image is not a URL, then the SD card is checked anyway.

### QR

The content is content of QR code, and size if the overall size (width and height) in pixels.

### HLine/VLine

Draws a horizontal or virtial line based on size and alignment.

### Bins

This allows display of bin collection. This is based on a JSON file, the content is the URL tyo fetch the JSON. A script `monmouthire.cgi` is defined for now.

Top level JSON, specifies next bin collection day.

|Field|Meaning|
|-----|-------|
|`baseurl`|Base URL for icons, default is system wide base url|
|`cache`|Datetime for caching this JSON, default is to `clear`, or 1 hour|
|`display`|Datetime when to display, default 5 days before `collect`|
|`leds`|Datetime for LEDs show, default is 12 hours before `collect`|
|`collect`|Datetime for collection, e.g. Monmouthshire is at `07:00:00`|
|`clear`|Datetime to clear dispaly, default is 12 hours after `collect`|
|`bins`|Array of bin objects for next collection|

Bins array has objects, and can have an empty object if needed (useful as arrays cannot have trailing commas).

|Field|Meaning|
|-----|-------|
|`name`|Name of bin, used as default if icon cannot be shown - so could be `Black`, or `General`, etc.|
|`colour`|Colour letter for LEDs|
|`icon`|Icon URL, can be tail url from `baseurl`|

Example.

```
{
	"baseurl":"http://epd.revk.uk",
	"collect":"2025-03-05 07:00:00",
	"clear":"2025-03-05 12:00:00",
	"bins":[
		{"name":"RED","colour":"R","icon":"Red.png"},
		{"name":"PURPLE","colour":"M","icon":"Purple.png"},
		{"name":"BLUE","colour":"B","icon":"Blue.png"},
		{}
	]
}
```

### More

More widgets planned

## Align, mask, and invert

For most widgets the *x*/*y* and *align* apply as expected. But there is also a mask/invert setting.

Invert simply swaps black/white.

Mask means only plot for asserted colour - content of text or white in am image. However, when dealing with an *image* the mask is not sensible when the PNG image has alpha as that is used as mask. Depending on the display and `gfx.invert` properties images may need to be inverted.
