@echo off
REM Comprehensive search for ALL OpenGL 1.2 functions removed in OpenGL 3.3+ Core Profile
REM Run this batch file in your Defcon source directory

echo.
echo 🔍 SEARCHING FOR DEPRECATED OPENGL 1.2 FUNCTIONS...
echo =================================================
echo.

REM Check if we're in the right directory
if not exist "source" (
    echo ❌ ERROR: 'source' directory not found!
    echo Please run this script from your Defcon root directory
    pause
    exit /b 1
)

if not exist "contrib\systemIV" (
    echo ❌ ERROR: 'contrib\systemIV' directory not found!
    echo Please run this script from your Defcon root directory
    pause
    exit /b 1
)

REM Create output file for results
set OUTPUT=opengl_deprecated_calls.txt
echo OpenGL 1.2 Deprecated Function Search Results > %OUTPUT%
echo Generated on %date% at %time% >> %OUTPUT%
echo ================================================= >> %OUTPUT%
echo. >> %OUTPUT%

REM ========================================
REM 1. IMMEDIATE MODE RENDERING (CRITICAL)
REM ========================================
echo 1. 🚨 IMMEDIATE MODE RENDERING (Must be replaced):
echo 1. IMMEDIATE MODE RENDERING (Must be replaced): >> %OUTPUT%
echo --------------------------------------------- >> %OUTPUT%
findstr /S /N /I "glBegin glEnd glVertex2f glVertex3f glColor3f glColor4f glNormal glTexCoord" source\*.cpp source\*.h contrib\systemIV\*.cpp contrib\systemIV\*.h 2>nul >> %OUTPUT%
findstr /S /N /I "glBegin glEnd glVertex2f glVertex3f glColor3f glColor4f" source\*.cpp source\*.h contrib\systemIV\*.cpp contrib\systemIV\*.h 2>nul

echo.
echo. >> %OUTPUT%

REM ========================================
REM 2. MATRIX STACK OPERATIONS (CRITICAL)
REM ========================================
echo 2. 🚨 MATRIX STACK OPERATIONS (Must be replaced):
echo 2. MATRIX STACK OPERATIONS (Must be replaced): >> %OUTPUT%
echo ---------------------------------------------- >> %OUTPUT%
findstr /S /N /I "glPushMatrix glPopMatrix glLoadIdentity glTranslatef glRotatef glScalef glLoadMatrixf glMultMatrixf" source\*.cpp source\*.h contrib\systemIV\*.cpp contrib\systemIV\*.h 2>nul >> %OUTPUT%
findstr /S /N /I "glPushMatrix glPopMatrix glLoadIdentity glTranslatef glRotatef glScalef" source\*.cpp source\*.h contrib\systemIV\*.cpp contrib\systemIV\*.h 2>nul

echo.
echo. >> %OUTPUT%

REM ========================================
REM 3. DISPLAY LISTS (HIGH PERFORMANCE IMPACT)
REM ========================================
echo 3. 🚨 DISPLAY LISTS (High performance impact):
echo 3. DISPLAY LISTS (High performance impact): >> %OUTPUT%
echo ------------------------------------------- >> %OUTPUT%
findstr /S /N /I "glNewList glEndList glCallList glCallLists glGenLists glDeleteLists glListBase glIsList" source\*.cpp source\*.h contrib\systemIV\*.cpp contrib\systemIV\*.h 2>nul >> %OUTPUT%
findstr /S /N /I "glNewList glEndList glCallList glGenLists glDeleteLists" source\*.cpp source\*.h contrib\systemIV\*.cpp contrib\systemIV\*.h 2>nul

echo.
echo. >> %OUTPUT%

REM ========================================
REM 4. FIXED FUNCTION PIPELINE - LIGHTING
REM ========================================
echo 4. ⚠️  FIXED FUNCTION LIGHTING:
echo 4. FIXED FUNCTION LIGHTING: >> %OUTPUT%
echo -------------------------- >> %OUTPUT%
findstr /S /N /I "glLight glMaterial glColorMaterial glLightModel" source\*.cpp source\*.h contrib\systemIV\*.cpp contrib\systemIV\*.h 2>nul >> %OUTPUT%
findstr /S /N /I "glLight glMaterial glColorMaterial" source\*.cpp source\*.h contrib\systemIV\*.cpp contrib\systemIV\*.h 2>nul

echo.
echo. >> %OUTPUT%

REM ========================================
REM 5. TEXTURE ENVIRONMENT (DEPRECATED)
REM ========================================
echo 5. ⚠️  TEXTURE ENVIRONMENT:
echo 5. TEXTURE ENVIRONMENT: >> %OUTPUT%
echo ------------------- >> %OUTPUT%
findstr /S /N /I "glTexEnv glTexGen" source\*.cpp source\*.h contrib\systemIV\*.cpp contrib\systemIV\*.h 2>nul >> %OUTPUT%
findstr /S /N /I "glTexEnv glTexGen" source\*.cpp source\*.h contrib\systemIV\*.cpp contrib\systemIV\*.h 2>nul

echo.
echo. >> %OUTPUT%

REM ========================================
REM 6. CLIENT STATE ARRAYS (DEPRECATED)
REM ========================================
echo 6. ⚠️  CLIENT STATE ARRAYS:
echo 6. CLIENT STATE ARRAYS: >> %OUTPUT%
echo ---------------------- >> %OUTPUT%
findstr /S /N /I "glEnableClientState glDisableClientState glVertexPointer glColorPointer glTexCoordPointer glNormalPointer" source\*.cpp source\*.h contrib\systemIV\*.cpp contrib\systemIV\*.h 2>nul >> %OUTPUT%
findstr /S /N /I "glEnableClientState glVertexPointer glColorPointer" source\*.cpp source\*.h contrib\systemIV\*.cpp contrib\systemIV\*.h 2>nul

echo.
echo. >> %OUTPUT%

REM ========================================
REM 7. ATTRIBUTE STACK OPERATIONS
REM ========================================
echo 7. ⚠️  ATTRIBUTE STACK:
echo 7. ATTRIBUTE STACK: >> %OUTPUT%
echo ------------------ >> %OUTPUT%
findstr /S /N /I "glPushAttrib glPopAttrib glPushClientAttrib glPopClientAttrib" source\*.cpp source\*.h contrib\systemIV\*.cpp contrib\systemIV\*.h 2>nul >> %OUTPUT%
findstr /S /N /I "glPushAttrib glPopAttrib" source\*.cpp source\*.h contrib\systemIV\*.cpp contrib\systemIV\*.h 2>nul

echo.
echo. >> %OUTPUT%

REM ========================================
REM 8. RASTER OPERATIONS
REM ========================================
echo 8. ⚠️  RASTER OPERATIONS:
echo 8. RASTER OPERATIONS: >> %OUTPUT%
echo -------------------- >> %OUTPUT%
findstr /S /N /I "glRasterPos glWindowPos glBitmap glDrawPixels glPixelZoom glCopyPixels" source\*.cpp source\*.h contrib\systemIV\*.cpp contrib\systemIV\*.h 2>nul >> %OUTPUT%
findstr /S /N /I "glRasterPos glBitmap glDrawPixels" source\*.cpp source\*.h contrib\systemIV\*.cpp contrib\systemIV\*.h 2>nul

echo.
echo. >> %OUTPUT%

REM ========================================
REM 9. RECTANGLE FUNCTIONS
REM ========================================
echo 9. ⚠️  RECTANGLE FUNCTIONS:
echo 9. RECTANGLE FUNCTIONS: >> %OUTPUT%
echo ---------------------- >> %OUTPUT%
findstr /S /N /I "glRectf glRecti glRectd glRects" source\*.cpp source\*.h contrib\systemIV\*.cpp contrib\systemIV\*.h 2>nul >> %OUTPUT%
findstr /S /N /I "glRect" source\*.cpp source\*.h contrib\systemIV\*.cpp contrib\systemIV\*.h 2>nul

echo.
echo. >> %OUTPUT%

REM ========================================
REM 10. ALPHA TEST (REMOVED)
REM ========================================
echo 10. ⚠️  ALPHA TEST:
echo 10. ALPHA TEST: >> %OUTPUT%
echo -------------- >> %OUTPUT%
findstr /S /N /I "glAlphaFunc" source\*.cpp source\*.h contrib\systemIV\*.cpp contrib\systemIV\*.h 2>nul >> %OUTPUT%
findstr /S /N /I "glAlphaFunc" source\*.cpp source\*.h contrib\systemIV\*.cpp contrib\systemIV\*.h 2>nul

echo.
echo. >> %OUTPUT%

REM ========================================
REM 11. ACCUMULATION BUFFER
REM ========================================
echo 11. ⚠️  ACCUMULATION BUFFER:
echo 11. ACCUMULATION BUFFER: >> %OUTPUT%
echo ----------------------- >> %OUTPUT%
findstr /S /N /I "glAccum glClearAccum" source\*.cpp source\*.h contrib\systemIV\*.cpp contrib\systemIV\*.h 2>nul >> %OUTPUT%
findstr /S /N /I "glAccum" source\*.cpp source\*.h contrib\systemIV\*.cpp contrib\systemIV\*.h 2>nul

echo.
echo. >> %OUTPUT%

REM ========================================
REM 12. STIPPLE PATTERNS
REM ========================================
echo 12. ⚠️  STIPPLE PATTERNS:
echo 12. STIPPLE PATTERNS: >> %OUTPUT%
echo -------------------- >> %OUTPUT%
findstr /S /N /I "glLineStipple glPolygonStipple" source\*.cpp source\*.h contrib\systemIV\*.cpp contrib\systemIV\*.h 2>nul >> %OUTPUT%
findstr /S /N /I "glLineStipple glPolygonStipple" source\*.cpp source\*.h contrib\systemIV\*.cpp contrib\systemIV\*.h 2>nul

echo.
echo. >> %OUTPUT%

REM ========================================
REM 13. GLU LIBRARY FUNCTIONS (REMOVED)
REM ========================================
echo 13. ⚠️  GLU LIBRARY FUNCTIONS:
echo 13. GLU LIBRARY FUNCTIONS: >> %OUTPUT%
echo ------------------------- >> %OUTPUT%
findstr /S /N /I "gluPerspective gluLookAt gluOrtho2D gluPickMatrix gluProject gluUnProject" source\*.cpp source\*.h contrib\systemIV\*.cpp contrib\systemIV\*.h 2>nul >> %OUTPUT%
findstr /S /N /I "gluPerspective gluLookAt gluOrtho2D" source\*.cpp source\*.h contrib\systemIV\*.cpp contrib\systemIV\*.h 2>nul

echo.
echo. >> %OUTPUT%

REM ========================================
REM 14. DEPRECATED GL CONSTANTS
REM ========================================
echo 14. ⚠️  DEPRECATED GL CONSTANTS:
echo 14. DEPRECATED GL CONSTANTS: >> %OUTPUT%
echo --------------------------- >> %OUTPUT%
findstr /S /N /I "GL_ALPHA_TEST GL_LIGHTING GL_COLOR_MATERIAL GL_NORMALIZE GL_MODELVIEW GL_PROJECTION glMatrixMode" source\*.cpp source\*.h contrib\systemIV\*.cpp contrib\systemIV\*.h 2>nul >> %OUTPUT%
findstr /S /N /I "GL_ALPHA_TEST GL_LIGHTING GL_MODELVIEW GL_PROJECTION glMatrixMode" source\*.cpp source\*.h contrib\systemIV\*.cpp contrib\systemIV\*.h 2>nul

echo.
echo. >> %OUTPUT%

REM ========================================
REM 15. RENDERER-SPECIFIC SEARCHES
REM ========================================
echo 15. 🔍 RENDERER-SPECIFIC PATTERNS:
echo 15. RENDERER-SPECIFIC PATTERNS: >> %OUTPUT%
echo ------------------------------- >> %OUTPUT%

echo Searching in renderer files specifically...
echo Renderer files: >> %OUTPUT%
if exist "source\renderer" (
    findstr /S /N /I "gl[A-Z]" source\renderer\*.cpp source\renderer\*.h 2>nul >> %OUTPUT%
    findstr /S /N /I "gl[A-Z]" source\renderer\*.cpp source\renderer\*.h 2>nul
) else (
    echo No renderer directory found >> %OUTPUT%
    echo No renderer directory found
)

echo.
echo. >> %OUTPUT%

REM ========================================
REM SUMMARY SECTION
REM ========================================
echo ================================================= >> %OUTPUT%
echo SEARCH COMPLETE! >> %OUTPUT%
echo ================================================= >> %OUTPUT%
echo. >> %OUTPUT%
echo PRIORITY ORDER FOR REPLACEMENT: >> %OUTPUT%
echo 1. 🚨 CRITICAL: Immediate mode (glBegin/glEnd/glVertex) >> %OUTPUT%
echo 2. 🚨 CRITICAL: Matrix operations (glPushMatrix/glPopMatrix) >> %OUTPUT%
echo 3. 🚨 HIGH: Display lists (glNewList/glCallList) >> %OUTPUT%
echo 4. ⚠️  MEDIUM: Fixed function pipeline >> %OUTPUT%
echo 5. ⚠️  LOW: Everything else >> %OUTPUT%
echo. >> %OUTPUT%
echo TIP: Focus on CRITICAL items first for conversion! >> %OUTPUT%

echo.
echo =================================================
echo 🎯 SEARCH COMPLETE!
echo =================================================
echo.
echo 📋 PRIORITY ORDER FOR REPLACEMENT:
echo 1. 🚨 CRITICAL: Immediate mode (glBegin/glEnd/glVertex)
echo 2. 🚨 CRITICAL: Matrix operations (glPushMatrix/glPopMatrix)
echo 3. 🚨 HIGH: Display lists (glNewList/glCallList)
echo 4. ⚠️  MEDIUM: Fixed function pipeline
echo 5. ⚠️  LOW: Everything else
echo.
echo 💡 TIP: Focus on CRITICAL items first for conversion!
echo.
echo 📄 Results saved to: %OUTPUT%
echo.
pause