                      Note for Microsoft Windows users.
                     -----------------------------------

Which executable
----------------
Start the executable from inside the bin folder: it reads data/eval.dat in the
current folder.

64-bit Windows on an x64 CPU:
  wEdax-x86-64.exe     any x64 CPU (SSE2, POPCNT not needed).
  wEdax-x86-64-v3.exe  CPUs of the x86-64-v3 level: AVX2, BMI1, BMI2, FMA,
                       LZCNT, MOVBE (Intel Haswell and later, AMD Zen and
                       later). It also uses POPCNT.
  wEdax-x86-64-v4.exe  CPUs of the x86-64-v4 level: AVX-512 F, BW, CD, DQ and
                       VL, in addition to the x86-64-v3 level (Intel Skylake-SP,
                       Ice Lake and later; AMD Zen 4 and later). It uses
                       256-bit vectors.
  If you are not sure, use wEdax-x86-64.exe.

32-bit Windows (or a 32-bit program on 64-bit Windows):
  wEdax-x86-sse.exe    x86 CPUs with SSE2.
  wEdax-x86.exe        x86 CPUs, also without SSE2.

Windows on ARM64:
  wEdax-arm64.exe

Edax runs at about half speed in 32 bits, because the program makes a heavy
use of 64-bit variables. So use a 32-bit version only if you do not have any
other choice.


Using Edax with NBoard
-----------------------
Edax can be run from NBoard, a graphical interface designed by Chris Welty.
You first need to download NBoard from :
http://othellogateway.com/ntest/Ntest/NBoardSetup.msi
Install it: run NBoardSetup.msi and follow the displayed
instructions.
Then copy the file NBoard.exe into the directory where wEdax is installed.
Rename the wEdax executable you use (for example wEdax-x86-64.exe) into
ntest.exe.
Now if you click on the NBoard.exe file, it will use Edax as its engine.

Using Edax with Winboard (alien version)
-----------------------------------------
Edax 4.2 can be run from a branch version of winboard supporting reversi as a chess variant. You can download it here:
http://hgm.nubati.net/WinBoard-Alien.zip
Unzip the file in a directory of your choice.
Then launch winboard, for example like this

winboard /fcp wEdax-x86-64.exe /fd [directory where you install wEdax-x86-64.exe] /variant="reversi"

