# Camera subsystem

The current backend provides two logical synthetic camera entries (`top`, `side`) at 160 by 120 grayscale. Preview events are PNG encoded at 2.5 frames per second. Captures are saved as PGM dataset images; the preview is a separate compressed image representation.

Folder import accepts common Qt-supported PNG/JPEG/BMP/TIFF/PGM files, converts each to grayscale PGM, and records the original filename in `manifest.jsonl`. No Basler SDK/pylon backend, serial mapping, exposure/gain support, physical enumeration, or hardware error path is implemented. The original reference camera wrapper was inspected read-only; its callback/grab implementation was not copied.
