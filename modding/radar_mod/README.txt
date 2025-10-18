Radar Mod creation guide
By KezzaMcFezza, bert_the_turtle

---------------------------------
       Table of Contents:
---------------------------------

source.xcf - contains all the territory bmps in their original size
blur.xcf - contains all of the radar range layers, final image creation
median_blur - my method of smoothing the radar overlays
ranges_original.csm - original script that bert created, not supported in new GIMP
radar_range.csm - tested on GIMP 3.0, does a blur expansion on the bmp
bomber_range.csm - tested on GIMP 3.0, does a blur expansion on the bmp
fighter_sub_range.csm - tested on GIMP 3.0, does a blur expansion on the bmp

---------------------------------
         How to use:
---------------------------------

First, place these scripts...

radar_range.csm 
bomber_range.csm 
fighter_sub_range.csm 

inside...

AppData\Roaming\GIMP\3.0\scripts

They should appear in the filters menu in GIMP at the bottom, under the defcon menu
Next it's time to actually create these radar overlays

But remember to restart GIMP after placing the scripts in here!

---------------------------------
        Time to do it:
---------------------------------

First open source.xcf
Then scale the entire image to 1024 x 570
This is crucial or else the radar ranges will not be accurate

Interpolation set to none works best

Now find a territory layer you want to create a radar layer for
make sure you have actually selected the layer in the layers panel

Once clicked and selected, goto filters, Defcon and then pick the range you want

Now once all 3 overlays have been created you must upscale the image/layers to...
4096 x 2280

Now we can get to blur.xcf

---------------------------------
    Creating the final image:
---------------------------------

Once blur.xcf is opened you can drag your 3 layers that were just created into this project
make sure you just drag and drop! do not move them afterwards as the alignment must stay
the way it is!

Now this is where things can lean towards personal preference
But the radar overlays will be jagged and aliased because of the upscaling

My method of counter-acting this was to use median blur

I have provided my median blur preset for anyone to use, however i am sure there are better
methods out there

So all you have to do is select the layer, and select by color
Then right click the layer and goto filters, blur, median blur

Then once that window is open there will be a presets section
at the top, the farthest right box with the triangle, click that
and then import my median blur preset.

---------------------------------
         Layer Settings:
---------------------------------

Now it's important to set the layers to the correct blend mode and transparency

So let's right click on your layers and click...
Edit Layer Attributes...

Once inside this menu change the following options...
Mode: Addition (I)
Secondary Mode: GIMP Face, not the reset symbol

Now that all the layers have this applied it's time for the transparency settings
Here is what each overlay's transparency needs to be set to...

Radar Ranges: 24.7
Bomber Ranges: 4.7
SubFighter Ranges: 9.8

Once all the layers have this applied that's it!

---------------------------------
           Exporting:
---------------------------------

Defcon expects RGB color formats without an alpha channel, so it's very
important to export the image with the following options...

Make sure the exported image is a BMP
Inside the Export Menu uncheck Write Color Information
And set the RGB format to 24 bit

---------------------------------
        Troubleshooting:
---------------------------------

If you get an error when trying to load the blur.bmp in defcon
it's most likely because you exported the image in 32 bit color
with an alpha channel

Or the image itself is the wrong dimensions
remember 4096 x 2280

For anything else ChatGPT can help you
I am not a genius :)












