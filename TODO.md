FL
FIX:
- [x] displaying 'labscene' instead of 'labmap'
- [x] preventing the execution of any file via 'OPEN FROM DISK'
- [x] fixed configuration for the positioning of the viewmodel and other LabStudio elements
MAKE:
- [x] test scene for Chapter 1 (assets/maps/site14_chapter1.labscene)
RESTORE:
- [x] LabStudio.exe

LAB
DELETE:
- [x] plugin system
FIX:
- [x] CMake Assets Shipping to not ship all assets like inwork or idk
- [x] only dll, delete .lib and exp
ADD:
- [x] real facesync with audiofiles & Dialogs
- [x] discrete high-performance GPU enforcement (NvOptimusEnablement / AmdPowerXpressRequestHighPerformance)
- [x] pre-shading (OpenGL Program Binary Disk Cache + PSO/uniform pre-warming)
- [x] native FXAA 3.11 in post-processing pipeline