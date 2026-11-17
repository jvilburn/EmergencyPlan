// Included in the installer's [Code] section, and in the probe that tests it.

function IsCodeCharacter(C: Char): Boolean;
begin
  Result := ((C >= 'A') and (C <= 'Z')) or ((C >= 'a') and (C <= 'z')) or ((C >= '0') and (C <= '9'))
    or (C = '-') or (C = '_');
end;

// The invitation code in the installer's own file name, as the server's join page names the
// download: WardPlanning-Setup-<document>.<seed>.exe, a document id of 22 characters, a dot
// and a seed of 43, all base64url. A browser may have added " (1)" before the extension.
// Empty when the name carries none.
function InvitationCode(): String;
var
  Name: String;
  I, P: Integer;
begin
  Result := '';
  Name := ExtractFileName(ExpandConstant('{srcexe}'));
  Name := Copy(Name, 1, Length(Name) - Length(ExtractFileExt(Name)));
  if (Length(Name) > 4) and (Name[Length(Name)] = ')') then
  begin
    P := Length(Name) - 1;
    while (P > 1) and (Name[P] >= '0') and (Name[P] <= '9') do
      P := P - 1;
    if (P > 2) and (Name[P] = '(') and (Name[P - 1] = ' ') then
      Name := Copy(Name, 1, P - 2);
  end;
  if Length(Name) < 66 then
    Exit;
  Name := Copy(Name, Length(Name) - 65, 66);
  if Name[23] <> '.' then
    Exit;
  for I := 1 to 66 do
    if (I <> 23) and not IsCodeCharacter(Name[I]) then
      Exit;
  Result := Name;
end;
