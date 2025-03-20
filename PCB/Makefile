all:	png stl

stl:	EPD/EPD.stl EPDBat/EPDBat.stl

png:	$(patsubst %.kicad_pcb,%.png,$(wildcard */*.kicad_pcb))

PCBCase/case: PCBCase/case.c
	make -C PCBCase

PCBCase/clean: PCBCase/clean.c
	make -C PCBCase

%.stl: %.scad
	echo "Making $@"
	/Applications/OpenSCAD.app/Contents/MacOS/OpenSCAD $< -o $@
	echo "Made $@"

update:
	-git pull
	git submodule update --init --recursive --remote
	-git commit -a -m "Library update"
	-git push

EPD/EPD.scad: EPD/EPD.kicad_pcb PCBCase/case Makefile PCBCase/case.scad
	PCBCase/case -o $@ $< --top=6 --bottom=1 --pcb=2 --case=2

EPDBat/EPDBat.scad: EPDBat/EPDBat.kicad_pcb PCBCase/case Makefile PCBCase/case.scad
	PCBCase/case -o $@ $< --top=6 --bottom=1 --pcb=2 --case=2 --ignore=J4

%.png:	%.kicad_pcb PCBCase/clean PCBCase/render Makefile
	PCBCase/render $<
