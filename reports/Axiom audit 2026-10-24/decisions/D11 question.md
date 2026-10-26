# D11. The camera on timelines and number lines

A 4.5-billion-year geology timeline then "zoom to the last 10,000 years"; a number line from 0 to 1,000,000 then
the interval [0, 1]. Axiom 2 forbids re-scaling, and the camera is the sanctioned way to look closer (it exists for
axes and plane, said in words: `camera zoom=8 on=p`, `fit=a,b`, `reset`). camera.cpp:20 refuses other schemas
("needs an axes or plane scene"). The structure auditor found this is the only axiom-compatible way to change scale
on a timeline, and also that timeline ranges are `float` (a 4.5-billion-year line labels "now" as 256). Design the
camera for one-dimensional schemas: what zoom/fit/follow/reset mean on a line, how ticks and numbers re-label at
the new scale (1D camera = re-ranging the visible window with the world fixed), eased, with the checker and labels
following; the `double` fix for data ranges; the readback (the camera spec is already read back for 2D). Material:
/private/tmp/claude-501/-Users-soheldanial-code-server-testing-visulaizations-tutor/37e07ea3-2a08-44c3-b78a-4c2380bf6126/scratchpad/audit-2/notes.md (#7, #8, r14-geology-camera.dan), markup/camera.cpp, markup/layout.cpp computeRanges (number
line/timeline), docs/HANDOFF.md §9 ("no camera pan for long arrays or timelines"). Axioms: 2 (the world's scale
never changes; the window does), 4 (eased), 6 (camera lines read back).
