// Included in the installer's [Code] section.

function IsCodeCharacter(C: Char): Boolean;
begin
  Result := ((C >= 'A') and (C <= 'Z')) or ((C >= 'a') and (C <= 'z')) or ((C >= '0') and (C <= '9'))
    or (C = '-') or (C = '_');
end;

// True when the 66 characters of Name from Start are an invitation code: a document id of 22
// characters, a dot and a seed of 43, all base64url.
function IsCodeAt(Name: String; Start: Integer): Boolean;
var
  I: Integer;
begin
  Result := False;
  if Name[Start + 22] <> '.' then
    Exit;
  for I := 0 to 65 do
    if (I <> 22) and not IsCodeCharacter(Name[Start + I]) then
      Exit;
  Result := True;
end;

// The invitation code in the installer's own file name, as the server's join page names the
// download: WardPlanning-Tanglewood-<document>.<seed>.exe. It is found wherever it sits in the
// name, so what a browser adds for a second download of one name, such as " (1)" or "(1)",
// is passed over. Empty when the name carries none.
function InvitationCode(): String;
var
  Name: String;
  Start: Integer;
begin
  Result := '';
  Name := ExtractFileName(ExpandConstant('{srcexe}'));
  for Start := Length(Name) - 65 downto 1 do
    if IsCodeAt(Name, Start) then
    begin
      Result := Copy(Name, Start, 66);
      Exit;
    end;
end;
