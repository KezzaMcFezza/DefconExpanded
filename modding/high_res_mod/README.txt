README PLEASE, OR I WILL BE SAD

-----------------------------------------------------------------------------------

recent updates:

the 4k mod is the mainstream mod that we are going to use
I have done some manual touchups and it looks really good

antartica is now fixed, still using the non Iceshelf version however
but the 4k version works on steam defcon without issue

there is a script that automatically grabs the shapefiles and makes bmps from them
so any coastline updates can automatically be transferred into bmp format

-----------------------------------------------------------------------------------

how to invoke the script:

python bmp_script.py --4k --lakes --no-ice

now heres the list of actual command line options

Resolution options, you can select one or multiple:
--4k   : create a 4096×2048 BMP
--8k   : create an 8192×4096 BMP
--16k  : create a 16384×8192 BMP
--32k  : create a 32768×16384 BMP

Lakes options:
--lakes    : include lakes 
--no-lakes : exclude lakes

Ice shelf options:
--yes-ice  : include Antarctic ice shelves
--no-ice   : exclude Antarctic ice shelves


