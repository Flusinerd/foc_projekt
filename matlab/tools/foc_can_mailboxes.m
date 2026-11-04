function foc_can_mailboxes()
%FOC_CAN_MAILBOXES  CAN-Sendebotschaften 0x110 und 0x111 auf getrennte Mailboxen legen.
%
%   foc_can_mailboxes()
%
%   In focLib/Input Controller senden "BatteryVoltage_CodeGen/eCAN Transmit" (ID 0x110, PVDD)
%   und "GVDD_Calculation/eCAN Transmit" (ID 0x111, GVDD) beide ueber eCAN_A, Mailbox 1
%   (Altmodell der MA). Beide Botschaften teilen sich damit ein Message-Objekt. Belegt sind
%   ausserdem die Empfangs-Mailboxen 9 (0x101) und 15 (0x102).
%   Neu: GVDD (0x111) auf Mailbox 2. PVDD (0x110) bleibt auf Mailbox 1.
%   Die eCAN-Bloecke wirken nur auf die Codegenerierung; die Simulation ist nicht betroffen.
%   app_speed verweist per Bibliotheksverweis auf focLib/Input Controller und uebernimmt die
%   Aenderung ohne eigene Speicherung. Vorher/nachher werden ausgegeben; focLib wird gespeichert.

root = fileparts(fileparts(mfilename('fullpath')));
lib = 'focLib';
if ~bdIsLoaded(lib), load_system(fullfile(root, 'models', [lib '.slx'])); end
set_param(lib, 'Lock', 'off');

base = [lib '/Input Controller'];
gvdd = [base '/GVDD_Calculation/eCAN Transmit'];
newMb = '2';
if getSimulinkBlockHandle(gvdd) == -1
    error('foc:can', 'Block nicht gefunden: %s', gvdd);
end
fprintf('vorher:\n');
showAll(base);
if strcmp(get_param(gvdd, 'mailboxNo'), newMb)
    fprintf('GVDD bereits auf Mailbox %s, keine Aenderung.\n', newMb);
    return;
end
used = mailboxes(base, gvdd);
if any(strcmp(used, newMb))
    error('foc:can', 'Mailbox %s ist bereits belegt - nicht geaendert.', newMb);
end
set_param(gvdd, 'mailboxNo', newMb);
fprintf('nachher:\n');
showAll(base);
save_system(lib);
fprintf('focLib gespeichert.\n');
end

function b = canBlocks(base)
b = find_system(base, 'LookUnderMasks', 'all', 'FollowLinks', 'off', ...
    'MatchFilter', @Simulink.match.allVariants, 'Regexp', 'on', 'MaskType', 'eCAN');
end

function showAll(base)
b = canBlocks(base);
for k = 1:numel(b)
    fprintf('  %-60s %-10s mailboxNo=%-3s messageID=%s\n', strrep(b{k}, [base '/'], ''), ...
        get_param(b{k}, 'useModule'), get_param(b{k}, 'mailboxNo'), get_param(b{k}, 'messageID'));
end
end

function used = mailboxes(base, except)
b = setdiff(canBlocks(base), {except});
used = cellfun(@(x) get_param(x, 'mailboxNo'), b, 'UniformOutput', false);
end
