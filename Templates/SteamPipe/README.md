# SteamPipe preparation

1. Get the AppID/DepotID and current Steamworks SDK access from the Steamworks
   partner account.
2. Copy the templates into a safe release working folder outside the project and
   fill the placeholders in there.
3. Never write a username, password, token or VDF auth material into the
   repository. Use SteamCMD's interactive login and Steam Guard.
4. Check placeholders, files and `preview=1` with
   `Scripts/Publish-Steam-DryRun.ps1` first.
5. A real upload happens only on an explicit request and with `-ConfirmUpload`.
6. `setlive` stays empty. Moving an uploaded build to a live branch is a separate
   step and needs its own explicit instruction.
