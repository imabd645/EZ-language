; ============================================================================
; EZ Language — Windows Installer
;
; Bundles ez.exe, its runtime DLLs (if dynamically linked), and the standard
; library (lib/) into a single install directory, so `use "db"` etc. resolve
; correctly relative to the interpreter with no separate setup step.
;
; Build with:
;   makensis /DPAYLOADDIR=<path-to-staged-build> /DAPPVERSION=5.0.1 installer.nsi
;
; PAYLOADDIR must contain:
;   ez.exe
;   *.dll        (optional — only present on a dynamically linked build)
;   lib/         (the standard library folder)
; ============================================================================

!include "MUI2.nsh"
!include "WinMessages.nsh"

!ifndef PAYLOADDIR
  !define PAYLOADDIR "payload"
!endif
!ifndef APPVERSION
  !define APPVERSION "0.0.0"
!endif

!define APPNAME "EZ Language"
!define APPEXE "ez.exe"
!define UNINSTKEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\EZLanguage"

Name "${APPNAME} ${APPVERSION}"
OutFile "ez-v${APPVERSION}-windows-x64-setup.exe"
InstallDir "$PROGRAMFILES64\EZ"
InstallDirRegKey HKLM "${UNINSTKEY}" "InstallLocation"
RequestExecutionLevel admin

; --- UI pages ---
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

; ----------------------------------------------------------------------------
Section "EZ Interpreter" SecMain
  SectionIn RO

  SetOutPath "$INSTDIR"
  File "${PAYLOADDIR}\${APPEXE}"
  ; /nonfatal: a statically-linked build ships no DLLs at all — that's a
  ; valid, expected outcome, not an install failure.
  File /nonfatal "${PAYLOADDIR}\*.dll"

  ; The standard library sits alongside the interpreter, not in a separate
  ; shared location — this is what "use \"db\"" etc. resolve against by
  ; default when no EZLIB_PATH override is set.
  SetOutPath "$INSTDIR\lib"
  File /r "${PAYLOADDIR}\lib\*.*"

  ; --- Uninstaller registration (Programs and Features) ---
  WriteUninstaller "$INSTDIR\uninstall.exe"
  WriteRegStr HKLM "${UNINSTKEY}" "DisplayName" "${APPNAME}"
  WriteRegStr HKLM "${UNINSTKEY}" "DisplayVersion" "${APPVERSION}"
  WriteRegStr HKLM "${UNINSTKEY}" "InstallLocation" "$INSTDIR"
  WriteRegStr HKLM "${UNINSTKEY}" "UninstallString" "$INSTDIR\uninstall.exe"
  WriteRegDWORD HKLM "${UNINSTKEY}" "NoModify" 1
  WriteRegDWORD HKLM "${UNINSTKEY}" "NoRepair" 1

  Call AddToPath
SectionEnd

; ----------------------------------------------------------------------------
; Appends $INSTDIR to the system PATH (HKLM, so it applies to every user)
; and broadcasts WM_SETTINGCHANGE so most already-open programs pick it up
; without a reboot. Simple append — if you reinstall over the same
; directory repeatedly without ever uninstalling, check for a duplicate
; entry before adding again.
Function AddToPath
  ReadRegStr $0 HKLM "SYSTEM\CurrentControlSet\Control\Session Manager\Environment" "Path"

  ; Skip if already present
  Push "$0"
  Push "$INSTDIR"
  Call StrContains
  Pop $1
  StrCmp $1 "1" done 0

  StrCpy $0 "$0;$INSTDIR"
  WriteRegExpandStr HKLM "SYSTEM\CurrentControlSet\Control\Session Manager\Environment" "Path" "$0"
  SendMessage ${HWND_BROADCAST} ${WM_SETTINGCHANGE} 0 "STR:Environment" /TIMEOUT=5000

  done:
FunctionEnd

; Tiny substring test: pushes "1" if the top string contains the second,
; else "0". Used only by AddToPath above.
Function StrContains
  Exch $R1 ; needle
  Exch
  Exch $R0 ; haystack
  Push $R2
  Push $R3
  Push $R4
  Push $R5

  StrLen $R2 $R1
  StrCpy $R4 0

  loop:
    StrCpy $R3 $R0 $R2 $R4
    StrCmp $R3 $R1 found
    StrCmp $R3 "" notfound
    IntOp $R4 $R4 + 1
    Goto loop

  found:
    StrCpy $R0 "1"
    Goto out
  notfound:
    StrCpy $R0 "0"

  out:
  Pop $R5
  Pop $R4
  Pop $R3
  Pop $R2
  Pop $R1
  Exch $R0
FunctionEnd

; ----------------------------------------------------------------------------
Section "Uninstall"
  RMDir /r "$INSTDIR"
  DeleteRegKey HKLM "${UNINSTKEY}"

  ; Best-effort PATH cleanup: rebuilds Path with every ";"-delimited segment
  ; that exactly equals $INSTDIR dropped. If InstallDir was changed between
  ; install and uninstall this won't catch the old entry -- harmless (a
  ; stale PATH segment pointing at a now-empty folder), just not fully tidy.
  Call un.RemoveFromPath
SectionEnd

; Walks the machine PATH one ";"-delimited segment at a time and rewrites it
; with any segment equal to $INSTDIR dropped. Simple linear scan -- no
; substring replace, no recursion, so there's nothing subtle to get wrong.
Function un.RemoveFromPath
  Push $0 ; full current PATH, then working remainder
  Push $1 ; rebuilt PATH
  Push $2 ; current segment
  Push $3 ; scan index within $0

  ReadRegStr $0 HKLM "SYSTEM\CurrentControlSet\Control\Session Manager\Environment" "Path"
  StrCpy $0 "$0;" ; trailing delimiter so every segment (incl. the last) ends in ";"
  StrCpy $1 ""

  next_segment:
    StrCmp $0 "" done
    StrCpy $3 0
    find_delim:
      StrCpy $2 $0 1 $3
      StrCmp $2 "" done            ; malformed / ran off the end -- stop, keep what we have
      StrCmp $2 ";" got_segment
      IntOp $3 $3 + 1
      Goto find_delim
    got_segment:
      StrCpy $2 $0 $3              ; segment text before the ";"
      IntOp $3 $3 + 1
      StrCpy $0 $0 "" $3           ; remainder after the ";"
      StrCmp $2 "$INSTDIR" next_segment  ; drop it -- don't append
      StrCmp $2 "" next_segment          ; also drop empty segments
      StrCpy $1 "$1$2;"
      Goto next_segment

  done:
    ; drop the trailing ";" this loop always adds (if anything was kept)
    StrCmp $1 "" write_back
    StrLen $3 $1
    IntOp $3 $3 - 1
    StrCpy $1 $1 $3

  write_back:
    WriteRegExpandStr HKLM "SYSTEM\CurrentControlSet\Control\Session Manager\Environment" "Path" "$1"
    SendMessage ${HWND_BROADCAST} ${WM_SETTINGCHANGE} 0 "STR:Environment" /TIMEOUT=5000

  Pop $3
  Pop $2
  Pop $1
  Pop $0
FunctionEnd
