// Generated case design for EPD/EPD.kicad_pcb
// By https://github.com/revk/PCBCase
// Generated 2025-11-01 14:31:56
// title:	Waveshare EPD adapter
// rev:	1
// company:	Adrian Kennard, Andrews & Arnold Ltd
//

// Globals
margin=0.200000;
lip=3.000000;
lipa=0;
lipt=2;
casewall=3.000000;
casebottom=1.000000;
casetop=6.000000;
bottomthickness=0.000000;
topthickness=0.000000;
fit=0.000000;
snap=0.150000;
edge=2.000000;
pcbthickness=1.200000;
function pcbthickness()=1.200000;
nohull=false;
hullcap=1.000000;
hulledge=1.000000;
useredge=true;
datex=0.000000;
datey=0.000000;
datet=0.500000;
dateh=3.000000;
datea=0;
date="2025-03-24";
datef="OCRB";
logox=0.000000;
logoy=0.000000;
logot=0.500000;
logoh=10.000000;
logoa=0;
logo="A";
logof="AJK";
spacing=44.000001;
pcbwidth=28.000001;
function pcbwidth()=28.000001;
pcblength=37.000001;
function pcblength()=37.000001;
originx=100.000000;
originy=89.500000;

module outline(h=pcbthickness,r=0){linear_extrude(height=h)offset(r=r)polygon(points=[[-13.000000,-18.500000],[13.000000,-18.500000],[13.382684,-18.423880],[13.707107,-18.207106],[13.923879,-17.882683],[14.000000,-17.500000],[14.000000,17.500000],[13.923881,17.882684],[13.707107,18.207107],[13.382684,18.423879],[13.000000,18.500000],[-12.999999,18.500000],[-13.382682,18.423880],[-13.707105,18.207106],[-13.923877,17.882683],[-13.999999,17.500000],[-14.000000,9.000000],[-13.923880,8.617318],[-13.707106,8.292895],[-13.382683,8.076123],[-13.000000,8.000000],[-9.799999,8.000000],[-9.541181,7.965925],[-9.300000,7.866025],[-9.092893,7.707107],[-8.933974,7.500000],[-8.834073,7.258819],[-8.799999,7.000000],[-8.799999,-8.000000],[-8.834074,-8.258819],[-8.933975,-8.500000],[-9.092893,-8.707106],[-9.300000,-8.866025],[-9.541180,-8.965926],[-9.799999,-9.000000],[-13.000000,-9.000000],[-13.382683,-9.076119],[-13.707106,-9.292893],[-13.923878,-9.617316],[-14.000000,-10.000000],[-14.000000,-17.500000],[-13.923880,-17.882683],[-13.707106,-18.207106],[-13.382683,-18.423878]],paths=[[0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43]]);}

module pcb(h=pcbthickness,r=0){linear_extrude(height=h)offset(r=r)polygon(points=[[-13.000000,-18.500000],[13.000000,-18.500000],[13.382684,-18.423880],[13.707107,-18.207106],[13.923879,-17.882683],[14.000000,-17.500000],[14.000000,17.500000],[13.923881,17.882684],[13.707107,18.207107],[13.382684,18.423879],[13.000000,18.500000],[-12.999999,18.500000],[-13.382682,18.423880],[-13.707105,18.207106],[-13.923877,17.882683],[-13.999999,17.500000],[-14.000000,9.000000],[-13.923880,8.617318],[-13.707106,8.292895],[-13.382683,8.076123],[-13.000000,8.000000],[-9.799999,8.000000],[-9.541181,7.965925],[-9.300000,7.866025],[-9.092893,7.707107],[-8.933974,7.500000],[-8.834073,7.258819],[-8.799999,7.000000],[-8.799999,-8.000000],[-8.834074,-8.258819],[-8.933975,-8.500000],[-9.092893,-8.707106],[-9.300000,-8.866025],[-9.541180,-8.965926],[-9.799999,-9.000000],[-13.000000,-9.000000],[-13.382683,-9.076119],[-13.707106,-9.292893],[-13.923878,-9.617316],[-14.000000,-10.000000],[-14.000000,-17.500000],[-13.923880,-17.882683],[-13.707106,-18.207106],[-13.382683,-18.423878]],paths=[[0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43]]);}
module C14(){translate([6.000000,17.100000,1.200000])rotate([0,0,90.000000])children();}
module part_C14(part=true,hole=false,block=false)
{
translate([6.000000,17.100000,1.200000])rotate([0,0,90.000000])m0(part,hole,block,casetop); // RevK:C_0402 C_0402_1005Metric (back)
};
module C3(){translate([5.150001,17.100000,1.200000])rotate([0,0,90.000000])children();}
module part_C3(part=true,hole=false,block=false)
{
translate([5.150001,17.100000,1.200000])rotate([0,0,90.000000])m0(part,hole,block,casetop); // RevK:C_0402 C_0402_1005Metric (back)
};
module C2(){translate([-0.500000,-8.900000,1.200000])children();}
module part_C2(part=true,hole=false,block=false)
{
translate([-0.500000,-8.900000,1.200000])m0(part,hole,block,casetop); // RevK:C_0402 C_0402_1005Metric (back)
};
module P1(){translate([4.200001,-14.299999,1.200000])children();}
module part_P1(part=true,hole=false,block=false)
{
};
module R9(){translate([10.400001,-5.500000,1.200000])rotate([0,0,180.000000])children();}
module part_R9(part=true,hole=false,block=false)
{
translate([10.400001,-5.500000,1.200000])rotate([0,0,180.000000])m1(part,hole,block,casetop); // RevK:R_0402_ R_0402_1005Metric (back)
};
module P2(){translate([1.700001,-14.299999,1.200000])children();}
module part_P2(part=true,hole=false,block=false)
{
};
module C4(){translate([3.450001,17.100000,1.200000])rotate([0,0,90.000000])children();}
module part_C4(part=true,hole=false,block=false)
{
translate([3.450001,17.100000,1.200000])rotate([0,0,90.000000])m0(part,hole,block,casetop); // RevK:C_0402 C_0402_1005Metric (back)
};
module C11(){translate([1.750000,17.100000,1.200000])rotate([0,0,90.000000])children();}
module part_C11(part=true,hole=false,block=false)
{
translate([1.750000,17.100000,1.200000])rotate([0,0,90.000000])m0(part,hole,block,casetop); // RevK:C_0402 C_0402_1005Metric (back)
};
module R6(){translate([-11.099999,12.100000,1.200000])rotate([0,0,180.000000])children();}
module part_R6(part=true,hole=false,block=false)
{
translate([-11.099999,12.100000,1.200000])rotate([0,0,180.000000])m1(part,hole,block,casetop); // RevK:R_0402_ R_0402_1005Metric (back)
};
module U4(){translate([-1.000000,-0.500000,1.200000])rotate([0,0,90.000000])children();}
module part_U4(part=true,hole=false,block=false)
{
translate([-1.000000,-0.500000,1.200000])rotate([0,0,90.000000])m2(part,hole,block,casetop); // U4
};
module D6(){translate([9.000000,-7.500000,1.200000])rotate([0,0,-10.000000])children();}
module part_D6(part=true,hole=false,block=false)
{
translate([9.000000,-7.500000,1.200000])rotate([0,0,-10.000000])m3(part,hole,block,casetop); // D6
};
module C5(){translate([-7.299999,-8.900000,1.200000])rotate([0,0,180.000000])children();}
module part_C5(part=true,hole=false,block=false)
{
translate([-7.299999,-8.900000,1.200000])rotate([0,0,180.000000])m4(part,hole,block,casetop); // RevK:C_0603 C_0603_1608Metric (back)
};
module Q1(){translate([11.700001,15.300000,1.200000])rotate([0,0,90.000000])children();}
module part_Q1(part=true,hole=false,block=false)
{
translate([11.700001,15.300000,1.200000])rotate([0,0,90.000000])m5(part,hole,block,casetop); // Q1
};
module C7(){translate([2.600000,17.100000,1.200000])rotate([0,0,90.000000])children();}
module part_C7(part=true,hole=false,block=false)
{
translate([2.600000,17.100000,1.200000])rotate([0,0,90.000000])m0(part,hole,block,casetop); // RevK:C_0402 C_0402_1005Metric (back)
};
module R3(){translate([-13.299999,11.200001,1.200000])rotate([0,0,-90.000000])children();}
module part_R3(part=true,hole=false,block=false)
{
translate([-13.299999,11.200001,1.200000])rotate([0,0,-90.000000])m1(part,hole,block,casetop); // RevK:R_0402_ R_0402_1005Metric (back)
};
module C10(){translate([10.400001,-1.599999,1.200000])rotate([0,0,180.000000])children();}
module part_C10(part=true,hole=false,block=false)
{
translate([10.400001,-1.599999,1.200000])rotate([0,0,180.000000])m4(part,hole,block,casetop); // RevK:C_0603 C_0603_1608Metric (back)
};
module J4(){translate([-10.549999,-10.632499,1.200000])children();}
module part_J4(part=true,hole=false,block=false)
{
translate([-10.549999,-10.632499,1.200000])m6(part,hole,block,casetop,2); // J4
};
module J6(){translate([-3.799999,-10.632499,1.200000])children();}
module part_J6(part=true,hole=false,block=false)
{
translate([-3.799999,-10.632499,1.200000])m6(part,hole,block,casetop,2); // J4
};
module C17(){translate([8.100000,14.000000,1.200000])rotate([0,0,180.000000])children();}
module part_C17(part=true,hole=false,block=false)
{
translate([8.100000,14.000000,1.200000])rotate([0,0,180.000000])m7(part,hole,block,casetop); // RevK:C_0805 C_0805_2012Metric (back)
};
module C9(){translate([7.900001,-2.299999,1.200000])rotate([0,0,-90.000000])children();}
module part_C9(part=true,hole=false,block=false)
{
translate([7.900001,-2.299999,1.200000])rotate([0,0,-90.000000])m4(part,hole,block,casetop); // RevK:C_0603 C_0603_1608Metric (back)
};
module C12(){translate([0.850000,17.100000,1.200000])rotate([0,0,90.000000])children();}
module part_C12(part=true,hole=false,block=false)
{
translate([0.850000,17.100000,1.200000])rotate([0,0,90.000000])m0(part,hole,block,casetop); // RevK:C_0402 C_0402_1005Metric (back)
};
module P3(){translate([-2.549999,-14.299999,1.200000])children();}
module part_P3(part=true,hole=false,block=false)
{
};
module C1(){translate([8.700001,16.800000,1.200000])rotate([0,0,90.000000])children();}
module part_C1(part=true,hole=false,block=false)
{
translate([8.700001,16.800000,1.200000])rotate([0,0,90.000000])m4(part,hole,block,casetop); // RevK:C_0603 C_0603_1608Metric (back)
};
module C21(){translate([-11.099999,13.200001,1.200000])children();}
module part_C21(part=true,hole=false,block=false)
{
translate([-11.099999,13.200001,1.200000])m0(part,hole,block,casetop); // RevK:C_0402 C_0402_1005Metric (back)
};
module R2(){translate([10.700001,17.200001,1.200000])rotate([0,0,-90.000000])children();}
module part_R2(part=true,hole=false,block=false)
{
translate([10.700001,17.200001,1.200000])rotate([0,0,-90.000000])m1(part,hole,block,casetop); // RevK:R_0402_ R_0402_1005Metric (back)
};
module JP2(){translate([-3.799999,16.800000,1.200000])children();}
module part_JP2(part=true,hole=false,block=false)
{
};
module R4(){translate([12.500000,-8.000000,1.200000])rotate([0,0,180.000000])children();}
module part_R4(part=true,hole=false,block=false)
{
translate([12.500000,-8.000000,1.200000])rotate([0,0,180.000000])m1(part,hole,block,casetop); // RevK:R_0402_ R_0402_1005Metric (back)
};
module D2(){translate([11.400001,8.300000,1.200000])rotate([0,0,180.000000])children();}
module part_D2(part=true,hole=false,block=false)
{
translate([11.400001,8.300000,1.200000])rotate([0,0,180.000000])m3(part,hole,block,casetop); // D6
};
module V4(){translate([-1.000000,-18.500000,1.200000])rotate([0,0,180.000000])children();}
module part_V4(part=true,hole=false,block=false)
{
};
module R5(){translate([12.500000,-7.099999,1.200000])rotate([0,0,180.000000])children();}
module part_R5(part=true,hole=false,block=false)
{
translate([12.500000,-7.099999,1.200000])rotate([0,0,180.000000])m1(part,hole,block,casetop); // RevK:R_0402_ R_0402_1005Metric (back)
};
module L2(){translate([10.400001,1.000000,1.200000])rotate([0,0,180.000000])children();}
module part_L2(part=true,hole=false,block=false)
{
translate([10.400001,1.000000,1.200000])rotate([0,0,180.000000])rotate([-0.000000,-0.000000,-90.000000])m8(part,hole,block,casetop); // RevK:L_4x4_ TYA4020 (back)
};
module R7(){translate([-11.099999,9.200001,1.200000])rotate([0,0,180.000000])children();}
module part_R7(part=true,hole=false,block=false)
{
translate([-11.099999,9.200001,1.200000])rotate([0,0,180.000000])m1(part,hole,block,casetop); // RevK:R_0402_ R_0402_1005Metric (back)
};
module J8(){translate([2.950001,-10.632499,1.200000])children();}
module part_J8(part=true,hole=false,block=false)
{
translate([2.950001,-10.632499,1.200000])m6(part,hole,block,casetop,2); // J4
};
module C19(){translate([-11.099999,10.300000,1.200000])children();}
module part_C19(part=true,hole=false,block=false)
{
translate([-11.099999,10.300000,1.200000])m0(part,hole,block,casetop); // RevK:C_0402 C_0402_1005Metric (back)
};
module R11(){translate([12.200001,-5.500000,1.200000])rotate([0,0,180.000000])children();}
module part_R11(part=true,hole=false,block=false)
{
translate([12.200001,-5.500000,1.200000])rotate([0,0,180.000000])m1(part,hole,block,casetop); // RevK:R_0402_ R_0402_1005Metric (back)
};
module D5(){translate([-0.900000,-9.900000,1.200000])rotate([0,0,180.000000])children();}
module part_D5(part=true,hole=false,block=false)
{
translate([-0.900000,-9.900000,1.200000])rotate([0,0,180.000000])m9(part,hole,block,casetop); // D5
};
module P5(){translate([-9.299999,-14.299999,1.200000])children();}
module part_P5(part=true,hole=false,block=false)
{
};
module R1(){translate([12.800000,17.200001,1.200000])rotate([0,0,90.000000])children();}
module part_R1(part=true,hole=false,block=false)
{
translate([12.800000,17.200001,1.200000])rotate([0,0,90.000000])m1(part,hole,block,casetop); // RevK:R_0402_ R_0402_1005Metric (back)
};
module C15(){translate([4.300000,17.100000,1.200000])rotate([0,0,90.000000])children();}
module part_C15(part=true,hole=false,block=false)
{
translate([4.300000,17.100000,1.200000])rotate([0,0,90.000000])m0(part,hole,block,casetop); // RevK:C_0402 C_0402_1005Metric (back)
};
module C8(){translate([12.900001,-2.299999,1.200000])rotate([0,0,-90.000000])children();}
module part_C8(part=true,hole=false,block=false)
{
translate([12.900001,-2.299999,1.200000])rotate([0,0,-90.000000])m4(part,hole,block,casetop); // RevK:C_0603 C_0603_1608Metric (back)
};
module J1(){translate([0.000000,9.800000,1.200000])rotate([0,0,180.000000])children();}
module part_J1(part=true,hole=false,block=false)
{
translate([0.000000,9.800000,1.200000])rotate([0,0,180.000000])m10(part,hole,block,casetop); // J1
};
module D3(){translate([11.400001,12.300000,1.200000])rotate([0,0,180.000000])children();}
module part_D3(part=true,hole=false,block=false)
{
translate([11.400001,12.300000,1.200000])rotate([0,0,180.000000])m3(part,hole,block,casetop); // D6
};
module R8(){translate([9.800000,17.200001,1.200000])rotate([0,0,-90.000000])children();}
module part_R8(part=true,hole=false,block=false)
{
translate([9.800000,17.200001,1.200000])rotate([0,0,-90.000000])m1(part,hole,block,casetop); // RevK:R_0402_ R_0402_1005Metric (back)
};
module R10(){translate([8.500000,-5.500000,1.200000])rotate([0,0,180.000000])children();}
module part_R10(part=true,hole=false,block=false)
{
translate([8.500000,-5.500000,1.200000])rotate([0,0,180.000000])m1(part,hole,block,casetop); // RevK:R_0402_ R_0402_1005Metric (back)
};
module C18(){translate([7.500000,4.600000,1.200000])rotate([0,0,90.000000])children();}
module part_C18(part=true,hole=false,block=false)
{
translate([7.500000,4.600000,1.200000])rotate([0,0,90.000000])m7(part,hole,block,casetop); // RevK:C_0805 C_0805_2012Metric (back)
};
module U3(){translate([10.400001,-3.599999,1.200000])rotate([0,0,180.000000])children();}
module part_U3(part=true,hole=false,block=false)
{
translate([10.400001,-3.599999,1.200000])rotate([0,0,180.000000])m11(part,hole,block,casetop); // RevK:SOT-23-6-MD8942 SOT-23-6 (back)
};
module D4(){translate([-12.000000,16.500000,1.200000])rotate([0,0,90.000000])children();}
module part_D4(part=true,hole=false,block=false)
{
translate([-12.000000,16.500000,1.200000])rotate([0,0,90.000000])m12(part,hole,block,casetop); // D4
};
module J7(){translate([8.210000,-13.299999,1.200000])rotate([0,0,90.000000])children();}
module part_J7(part=true,hole=false,block=false)
{
translate([8.210000,-13.299999,1.200000])rotate([0,0,90.000000])translate([0.000000,-2.400000,0.000000])rotate([90.000000,-0.000000,-0.000000])m13(part,hole,block,casetop); // RevK:USB-C-Socket-H CSP-USC16-TR (back)
};
module P6(){translate([-11.799999,-14.299999,1.200000])children();}
module part_P6(part=true,hole=false,block=false)
{
};
module V1(){translate([14.000000,-1.000000,1.200000])rotate([0,0,-90.000000])children();}
module part_V1(part=true,hole=false,block=false)
{
};
module L1(){translate([10.700001,5.100000,1.200000])rotate([0,0,180.000000])children();}
module part_L1(part=true,hole=false,block=false)
{
translate([10.700001,5.100000,1.200000])rotate([0,0,180.000000])rotate([-0.000000,-0.000000,-90.000000])m8(part,hole,block,casetop); // RevK:L_4x4_ TYA4020 (back)
};
module D1(){translate([11.400001,10.300000,1.200000])rotate([0,0,180.000000])children();}
module part_D1(part=true,hole=false,block=false)
{
translate([11.400001,10.300000,1.200000])rotate([0,0,180.000000])m3(part,hole,block,casetop); // D6
};
module D7(){translate([-7.700000,-9.900000,1.200000])rotate([0,0,180.000000])children();}
module part_D7(part=true,hole=false,block=false)
{
translate([-7.700000,-9.900000,1.200000])rotate([0,0,180.000000])m9(part,hole,block,casetop); // D5
};
module P4(){translate([-5.049999,-14.299999,1.200000])children();}
module part_P4(part=true,hole=false,block=false)
{
};
module C6(){translate([-13.299999,9.500000,1.200000])rotate([0,0,-90.000000])children();}
module part_C6(part=true,hole=false,block=false)
{
translate([-13.299999,9.500000,1.200000])rotate([0,0,-90.000000])m0(part,hole,block,casetop); // RevK:C_0402 C_0402_1005Metric (back)
};
module PCB1(){translate([-1.000000,-1.000000,1.200000])children();}
module part_PCB1(part=true,hole=false,block=false)
{
};
// Parts to go on PCB (top)
module parts_top(part=false,hole=false,block=false){
part_C14(part,hole,block);
part_C3(part,hole,block);
part_C2(part,hole,block);
part_P1(part,hole,block);
part_R9(part,hole,block);
part_P2(part,hole,block);
part_C4(part,hole,block);
part_C11(part,hole,block);
part_R6(part,hole,block);
part_U4(part,hole,block);
part_D6(part,hole,block);
part_C5(part,hole,block);
part_Q1(part,hole,block);
part_C7(part,hole,block);
part_R3(part,hole,block);
part_C10(part,hole,block);
part_J4(part,hole,block);
part_J6(part,hole,block);
part_C17(part,hole,block);
part_C9(part,hole,block);
part_C12(part,hole,block);
part_P3(part,hole,block);
part_C1(part,hole,block);
part_C21(part,hole,block);
part_R2(part,hole,block);
part_JP2(part,hole,block);
part_R4(part,hole,block);
part_D2(part,hole,block);
part_V4(part,hole,block);
part_R5(part,hole,block);
part_L2(part,hole,block);
part_R7(part,hole,block);
part_J8(part,hole,block);
part_C19(part,hole,block);
part_R11(part,hole,block);
part_D5(part,hole,block);
part_P5(part,hole,block);
part_R1(part,hole,block);
part_C15(part,hole,block);
part_C8(part,hole,block);
part_J1(part,hole,block);
part_D3(part,hole,block);
part_R8(part,hole,block);
part_R10(part,hole,block);
part_C18(part,hole,block);
part_U3(part,hole,block);
part_D4(part,hole,block);
part_J7(part,hole,block);
part_P6(part,hole,block);
part_V1(part,hole,block);
part_L1(part,hole,block);
part_D1(part,hole,block);
part_D7(part,hole,block);
part_P4(part,hole,block);
part_C6(part,hole,block);
part_PCB1(part,hole,block);
}

parts_top=13;
module P7(){translate([4.200001,-14.299999,0.000000])rotate([0,0,180.000000])rotate([180,0,0])children();}
module part_P7(part=true,hole=false,block=false)
{
};
module V6(){translate([-1.000000,18.500000,0.000000])rotate([0,0,180.000000])rotate([180,0,0])children();}
module part_V6(part=true,hole=false,block=false)
{
};
module P11(){translate([-9.299999,-14.299999,0.000000])rotate([0,0,180.000000])rotate([180,0,0])children();}
module part_P11(part=true,hole=false,block=false)
{
};
module JP1(){translate([11.400001,17.000000,0.000000])rotate([180,0,0])children();}
module part_JP1(part=true,hole=false,block=false)
{
};
module V3(){translate([-14.000000,-1.000000,0.000000])rotate([0,0,-90.000000])rotate([180,0,0])children();}
module part_V3(part=true,hole=false,block=false)
{
};
module P12(){translate([-11.799999,-14.299999,0.000000])rotate([0,0,180.000000])rotate([180,0,0])children();}
module part_P12(part=true,hole=false,block=false)
{
};
module P8(){translate([1.700001,-14.299999,0.000000])rotate([0,0,180.000000])rotate([180,0,0])children();}
module part_P8(part=true,hole=false,block=false)
{
};
module P9(){translate([-2.549999,-14.299999,0.000000])rotate([0,0,180.000000])rotate([180,0,0])children();}
module part_P9(part=true,hole=false,block=false)
{
};
module P10(){translate([-5.049999,-14.299999,0.000000])rotate([0,0,180.000000])rotate([180,0,0])children();}
module part_P10(part=true,hole=false,block=false)
{
};
module J3(){translate([3.100000,-0.599999,0.000000])rotate([0,0,90.000000])rotate([180,0,0])children();}
module part_J3(part=true,hole=false,block=false)
{
};
module J2(){translate([-4.349999,-0.500000,0.000000])rotate([0,0,-90.000000])rotate([180,0,0])children();}
module part_J2(part=true,hole=false,block=false)
{
};
// Parts to go on PCB (bottom)
module parts_bottom(part=false,hole=false,block=false){
part_P7(part,hole,block);
part_V6(part,hole,block);
part_P11(part,hole,block);
part_JP1(part,hole,block);
part_V3(part,hole,block);
part_P12(part,hole,block);
part_P8(part,hole,block);
part_P9(part,hole,block);
part_P10(part,hole,block);
part_J3(part,hole,block);
part_J2(part,hole,block);
}

parts_bottom=0;
module b(cx,cy,z,w,l,h){translate([cx-w/2,cy-l/2,z])cube([w,l,h]);}
module m0(part=false,hole=false,block=false,height)
{ // RevK:C_0402 C_0402_1005Metric
// 0402 Capacitor
if(part)
{
	b(0,0,0,1.0,0.5,1); // Chip
	b(0,0,0,1.5,0.65,0.2); // Pad size
}
}

module m1(part=false,hole=false,block=false,height)
{ // RevK:R_0402_ R_0402_1005Metric
// 0402 Resistor
if(part)
{
	b(0,0,0,1.5,0.65,0.2); // Pad size
	b(0,0,0,1.0,0.5,0.5); // Chip
}
}

module m2(part=false,hole=false,block=false,height)
{ // U4
// ESP32-S3-MINI-1
translate([-15.4/2,-15.45/2,0])
{
	if(part)
	{
		cube([15.4,20.5,0.8]);
		translate([0.7,0.5,0])cube([14,13.55,2.4]);
		cube([15.4,20.5,0.8]);
	}
}
}

module m3(part=false,hole=false,block=false,height)
{ // D6
// SOD-123 Diode
if(part)
{
	b(0,0,0,2.85,1.8,1.35); // part
	b(0,0,0,4.2,1.2,0.7); // pads
}
}

module m4(part=false,hole=false,block=false,height)
{ // RevK:C_0603 C_0603_1608Metric
// 0603 Capacitor
if(part)
{
	b(0,0,0,1.6,0.8,1); // Chip
	b(0,0,0,1.6,0.95,0.2); // Pad size
}
}

module m5(part=false,hole=false,block=false,height)
{ // Q1
// SOT-23
if(part)
{
	b(0,0,0,1.4,3.0,1.1); // Body
	b(-0.9375,-0.95,0,1.475,0.6,0.5); // Pad
	b(-0.9375,0.95,0,1.475,0.6,0.5); // Pad
	b(0.9375,0,0,1.475,0.6,0.5); // Pad
}
}

module m6(part=false,hole=false,block=false,height,N=0)
{ // J4
// PTSM socket
if(part)
{
	hull()
	{
		b(0,-7.5/2+0.3,0,1.7+N*2.5,7.5,4);
		b(0,-7.5/2+0.3,0,1.7+N*2.5-2,7.5,5);
	}
	// Pins
	for(p=[0:N-1])translate([-2.5*(N-1)/2+p*2.5,0,-2.1])cylinder(r1=0.3,r2=1,h=2.1);
}
if(hole)
{
	b(0,-10.5/2-7.5+0.3,0,1.1+N*2.5,10.5,5);
}
}

module m7(part=false,hole=false,block=false,height)
{ // RevK:C_0805 C_0805_2012Metric
// 0805 Capacitor
if(part)
{
	b(0,0,0,2,1.2,1); // Chip
	b(0,0,0,2,1.45,0.2); // Pad size
}
}

module m8(part=false,hole=false,block=false,height)
{ // RevK:L_4x4_ TYA4020
// 4x4 Inductor
if(part)
{
	b(0,0,0,4,4,3);
}
}

module m9(part=false,hole=false,block=false,height)
{ // D5
// DFN1006-2L
if(part)
{
	b(0,0,0,1.0,0.6,0.45); // Chip
}
}

module m10(part=false,hole=false,block=false,height)
{ // J1
// FPC 24 pin connector
{
	if(part)
	{
		b(0,0,0,11.8,3.9,0.5); // Pins
		b(0,-0.6,0,15.8,5.2,2.1); // Connector
		b(0,0,1,15,25,0.1); // Ribbon
	}
	if(hole)
	{
		b(0,0,1,15,25,0.1); // Ribbon
	}
}
}

module m11(part=false,hole=false,block=false,height)
{ // RevK:SOT-23-6-MD8942 SOT-23-6
// SOT-23-6
if(part)
{
	b(0,0,0,1.726,3.026,1.2); // Part
	b(0,0,0,3.6,2.5,0.5); // Pins
}
}

module m12(part=false,hole=false,block=false,height)
{ // D4
// 1x1mm LED
if(part)
{
        b(0,0,0,1.2,1.2,.8);
}
if(hole)
{
        hull()
        {
                b(0,0,.8,1.2,1.2,1);
                translate([0,0,height])cylinder(d=1.001,h=0.001,$fn=16);
        }
}
if(block)
{
        hull()
        {
                b(0,0,.8,2.8,2.8,1);
                translate([0,0,height])cylinder(d=2,h=1,$fn=16);
        }
}
}

module m13(part=false,hole=false,block=false,height)
{ // RevK:USB-C-Socket-H CSP-USC16-TR
// USB connector
rotate([-90,0,0])translate([0,1.9,0])
{
	if(part)
	{
        b(0,1.57,0,7,1.14,0.2); // Pads

		translate([0,1.76-7.55,1.63])
		rotate([-90,0,0])
		hull()
		{
			translate([(8.94-3.26)/2,0,0])
			cylinder(d=3.26,h=7.55,$fn=24);
			translate([-(8.94-3.26)/2,0,0])
			cylinder(d=3.26,h=7.55,$fn=24);
		}
		translate([-8.94/2,0.99-1.1/2,0])cube([8.94,1.1,1.6301]);
		translate([-8.94/2,-3.2-1.6/2,0])cube([8.94,1.6,1.6301]);
	}
	if(hole)
		translate([0,-5.79,1.63])
		rotate([-90,0,0])
	{
		// Plug
		hull()
		{
			translate([(8.34-2.5)/2,0,-23+1])
			cylinder(d=2.5,h=23,$fn=24);
			translate([-(8.34-2.5)/2,0,-23+1])
			cylinder(d=2.5,h=23,$fn=24);
		}
		hull()
		{
            translate([(12-7)/2,0,-21-1])
			cylinder(d=7,h=21,$fn=24);
            translate([-(12-7)/2,0,-21-1])
			cylinder(d=7,h=21,$fn=24);
		}
		translate([0,0,-100-10])
			cylinder(d=5,h=100,$fn=24);
	}
}
}

// Generate PCB casework

height=casebottom+pcbthickness+casetop;
$fn=48;

module pyramid()
{ // A pyramid
 polyhedron(points=[[0,0,0],[-height,-height,height],[-height,height,height],[height,height,height],[height,-height,height]],faces=[[0,1,2],[0,2,3],[0,3,4],[0,4,1],[4,3,2,1]]);
}


module pcb_hulled(h=pcbthickness,r=0)
{ // PCB shape for case
	if(useredge)outline(h,r);
	else hull()outline(h,r);
}

module solid_case(d=0)
{ // The case wall
	hull()
        {
                translate([0,0,-casebottom])pcb_hulled(height,casewall-edge);
                translate([0,0,edge-casebottom])pcb_hulled(height-edge*2,casewall);
        }
}

module preview()
{
	pcb();
	color("#0f0")parts_top(part=true);
	color("#0f0")parts_bottom(part=true);
	color("#f00")parts_top(hole=true);
	color("#f00")parts_bottom(hole=true);
	color("#00f8")parts_top(block=true);
	color("#00f8")parts_bottom(block=true);
}

module top_half(fit=0)
{
	difference()
	{
		translate([-casebottom-100,-casewall-100,pcbthickness+0.01]) cube([pcbwidth+casewall*2+200,pcblength+casewall*2+200,height]);
		translate([0,0,pcbthickness])
        	{
			snape=lip/5;
			snaph=(lip-snape*2)/3;
			if(lipt==1)rotate(lipa)hull()
			{
				translate([0,-pcblength,lip/2])cube([0.001,pcblength*2,0.001]);
				translate([-lip/2,-pcblength,0])cube([lip,pcblength*2,0.001]);
			} else if(lipt==2)for(a=[0,90,180,270])rotate(a+lipa)hull()
			{
				translate([0,-pcblength-pcbwidth,lip/2])cube([0.001,pcblength*2+pcbwidth*2,0.001]);
				translate([-lip/2,-pcblength-pcbwidth,0])cube([lip,pcblength*2+pcbwidth*2,0.001]);
			}
            		difference()
            		{
                		pcb_hulled(lip,casewall);
				if(snap)
                        	{
					hull()
					{
						pcb_hulled(0.1,casewall/2-snap/2+fit);
						translate([0,0,snape])pcb_hulled(snaph,casewall/2+snap/2+fit);
						translate([0,0,lip-snape-snaph])pcb_hulled(0.1,casewall/2-snap/2+fit);
					}
					translate([0,0,lip-snape-snaph])pcb_hulled(snaph,casewall/2-snap/2+fit);
					hull()
					{
						translate([0,0,lip-snape])pcb_hulled(0.1,casewall/2-snap/2+fit);
						translate([0,0,lip])pcb_hulled(0.1,casewall/2+snap/2+fit);
					}
                        	}
				else pcb_hulled(lip,casewall/2+fit);
				if(lipt==0)translate([-pcbwidth,-pcblength,0])cube([pcbwidth*2,pcblength*2,lip]);
				else if(lipt==1) rotate(lipa)translate([0,-pcblength,0])hull()
				{
					translate([lip/2,0,0])cube([pcbwidth,pcblength*2,lip]);
					translate([-lip/2,0,lip])cube([pcbwidth,pcblength*2,lip]);
				}
				else if(lipt==2)for(a=[0,180])rotate(a+lipa)hull()
                		{
                            		translate([lip/2,lip/2,0])cube([pcbwidth+pcblength,pcbwidth+pcblength,lip]);
                            		translate([-lip/2,-lip/2,lip])cube([pcbwidth+pcblength,pcbwidth+pcblength,lip]);
                		}
            		}
            		difference()
            		{
				if(snap)
                        	{
					hull()
					{
						translate([0,0,-0.1])pcb_hulled(0.1,casewall/2+snap/2-fit);
						translate([0,0,snape-0.1])pcb_hulled(0.1,casewall/2-snap/2-fit);
					}
					translate([0,0,snape])pcb_hulled(snaph,casewall/2-snap/2-fit);
					hull()
					{
						translate([0,0,snape+snaph])pcb_hulled(0.1,casewall/2-snap/2-fit);
						translate([0,0,lip-snape-snaph])pcb_hulled(snaph,casewall/2+snap/2-fit);
						translate([0,0,lip-0.1])pcb_hulled(0.1,casewall/2-snap/2-fit);
					}
                        	}
				else pcb_hulled(lip,casewall/2-fit);
				if(lipt==1)rotate(lipa+180)translate([0,-pcblength,0])hull()
				{
					translate([lip/2,0,0])cube([pcbwidth,pcblength*2,lip+0.1]);
					translate([-lip/2,0,lip])cube([pcbwidth,pcblength*2,lip+0.1]);
				}
				else if(lipt==2)for(a=[90,270])rotate(a+lipa)hull()
                		{
                            		translate([lip/2,lip/2,0])cube([pcbwidth+pcblength,pcbwidth+pcblength,lip]);
                            		translate([-lip/2,-lip/2,lip])cube([pcbwidth+pcblength,pcbwidth+pcblength,lip]);
                		}
			}
            	}
		minkowski()
                {
                	union()
                	{
                		parts_top(part=true,hole=true);
                		parts_bottom(part=true,hole=true);
                	}
                	translate([-0.01,-0.01,-height])cube([0.02,0.02,height]);
                }
        }
	minkowski()
        {
        	union()
                {
                	parts_top(part=true,hole=true);
                	parts_bottom(part=true,hole=true);
                }
                translate([-0.01,-0.01,0])cube([0.02,0.02,height]);
        }
}

module case_wall()
{
	difference()
	{
		solid_case();
		translate([0,0,-height])pcb_hulled(height*2,0.02);
	}
}

module top_side_hole()
{
	difference()
	{
		intersection()
		{
			parts_top(hole=true);
			case_wall();
		}
		translate([0,0,-casebottom])pcb_hulled(height,casewall);
	}
}

module bottom_side_hole()
{
	difference()
	{
		intersection()
		{
			parts_bottom(hole=true);
			case_wall();
		}
		translate([0,0,edge-casebottom])pcb_hulled(height-edge*2,casewall);
	}
}

module parts_space()
{
	minkowski()
	{
		union()
		{
			parts_top(part=true,hole=true);
			parts_bottom(part=true,hole=true);
		}
		sphere(r=margin,$fn=6);
	}
}

module top_cut(fit=0)
{
	difference()
	{
		top_half(fit);
		if(parts_top)difference()
		{
			minkowski()
			{ // Penetrating side holes
				top_side_hole();
				rotate([180,0,0])
				pyramid();
			}
			minkowski()
			{
				top_side_hole();
				rotate([0,0,45])cylinder(r=margin,h=height,$fn=4);
			}
		}
	}
	if(parts_bottom)difference()
	{
		minkowski()
		{ // Penetrating side holes
			bottom_side_hole();
			pyramid();
		}
			minkowski()
			{
				bottom_side_hole();
				rotate([0,0,45])translate([0,0,-height])cylinder(r=margin,h=height,$fn=4);
			}
	}
}

module bottom_cut()
{
	difference()
	{
		 translate([-casebottom-50,-casewall-50,-height]) cube([pcbwidth+casewall*2+100,pcblength+casewall*2+100,height*2]);
		 top_cut(-fit);
	}
}

module top_body()
{
	difference()
	{
		intersection()
		{
			solid_case();
			pcb_hulled(casetop+pcbthickness,0.03);
		}
		if(parts_top||topthickness)minkowski()
		{
			union()
			{
				if(nohull)parts_top(part=true);
				else hull(){parts_top(part=true);pcb_hulled();}
				if(topthickness)pcb_hulled(casetop+pcbthickness-topthickness,0);
			}
			translate([0,0,margin-height])cylinder(r=margin,h=height,$fn=8);
		}
	}
	intersection()
	{
		pcb_hulled(casetop+pcbthickness,0.03);
		union()
		{
			parts_top(block=true);
			parts_bottom(block=true);
		}
	}
}

module top_edge()
{
	intersection()
	{
		case_wall();
		top_cut();
	}
}

module top_pos()
{ // Position for plotting bottom
	translate([0,0,pcbthickness+casetop])rotate([180,0,0])children();
}

module pcb_pos()
{	// Position PCB relative to base 
		translate([0,0,pcbthickness-height])children();
}

module top()
{
	top_pos()difference()
	{
		union()
		{
			top_body();
			top_edge();
		}
		parts_space();
		pcb_pos()pcb(height,r=margin);
	}
}

module bottom_body()
{ // Position for plotting top
	difference()
	{
		intersection()
		{
			solid_case();
			translate([0,0,-casebottom])pcb_hulled(casebottom+pcbthickness,0.03);
		}
		if(parts_bottom||bottomthickness)minkowski()
		{
			union()
			{
				if(nohull)parts_bottom(part=true);
				else hull()parts_bottom(part=true);
				if(bottomthickness)translate([0,0,bottomthickness-casebottom])pcb_hulled(casebottom+pcbthickness-bottomthickness,0);
			}
			translate([0,0,-margin])cylinder(r=margin,h=height,$fn=8);
		}
	}
	intersection()
	{
		translate([0,0,-casebottom])pcb_hulled(casebottom+pcbthickness,0.03);
		union()
		{
			parts_top(block=true);
			parts_bottom(block=true);
		}
	}
}

module bottom_edge()
{
	intersection()
	{
		case_wall();
		bottom_cut();
	}
}

module bottom_pos()
{
	translate([0,0,casebottom])children();
}

module bottom()
{
	bottom_pos()difference()
	{
		union()
		{
        		bottom_body();
        		bottom_edge();
		}
		parts_space();
		pcb(height,r=margin);
	}
}

module datecode()
{
	minkowski()
	{
		translate([datex,datey,-1])rotate(datea)scale([-1,1])linear_extrude(1)text(date,size=dateh,halign="center",valign="center",font=datef);
		cylinder(d1=datet,d2=0,h=datet,$fn=6);
	}
}

module logocode()
{
	minkowski()
	{
		translate([logox,logoy,-1])rotate(logoa)scale([-1,1])linear_extrude(1)text(logo,size=logoh,halign="center",valign="center",font=logof);
		cylinder(d1=logot,d2=0,h=logot,$fn=6);
	}
}
difference(){bottom();datecode();}translate([spacing,0,0])top();
