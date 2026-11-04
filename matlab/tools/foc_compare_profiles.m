function foc_compare_profiles(pa, pb)
%FOC_COMPARE_PROFILES  Alle Skalar-/Vektorfelder zweier Konfigurationsprofile gegenueberstellen.
%   foc_compare_profiles('legacy_f2838', 'datasheet')  gibt nur abweichende Groessen aus.
if nargin < 1, pa = 'legacy_f2838'; end
if nargin < 2, pb = 'datasheet'; end
a = foc_config(pa); b = foc_config(pb);
fa = flat(a.vars, ''); fb = flat(b.vars, '');
keys = union(fieldnames(fa), fieldnames(fb));
fprintf('%-44s %-22s %-22s\n', 'Groesse', pa, pb);
for k = 1:numel(keys)
    n = keys{k};
    va = '-'; vb = '-';
    if isfield(fa, n), va = fa.(n); end
    if isfield(fb, n), vb = fb.(n); end
    if ~isequal(va, vb)
        fprintf('%-44s %-22s %-22s\n', strrep(n, '__', '.'), str(va), str(vb));
    end
end
end

function f = flat(s, p)
f = struct();
fn = fieldnames(s);
for k = 1:numel(fn)
    v = s.(fn{k}); n = [p fn{k}];
    if isstruct(v) && isscalar(v)
        g = flat(v, [n '__']); gn = fieldnames(g);
        for j = 1:numel(gn), f.(gn{j}) = g.(gn{j}); end
    elseif isnumeric(v) || islogical(v) || ischar(v)
        f.(n) = v;
    else
        f.(n) = class(v);
    end
end
end

function s = str(v)
if ischar(v), s = v; elseif numel(v) == 1, s = sprintf('%.6g', double(v)); else, s = mat2str(double(v), 5); end
if numel(s) > 22, s = [s(1:19) '...']; end
end
