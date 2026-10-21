# santiNodes

## Addon split

The vector graphics models `Bar Maker`, `Generative Grid`, `Generative Grid 2`,
`Path Maker`, `Trim Group Paths`, and `Trim Path Sequential` are now registered by
`ofxOceanodeVectorGraphics`. The three TTS models (`Catotron TTS`, `OpenAI TTS`,
and `Aina TTS`) are now registered by `ofxOceanodeTTS`. Add those addons and call
their `registerModels` functions when loading presets that use these models.

The Thalastasi models and Snapshot Client/Server are no longer registered.
`rotoControlConfig` is available when the project adds `ofxOceanodeMidi`, which
defines `OFXOCEANODE_USE_MIDI`.

## Pitch Map

The **Pitch Map** node (Santi/Pitch category) expands an incoming ordered
`Scale` of semitone offsets (0–11) using PolyComb's oscillator pitch mapper.
It runs entirely in C++, without a SuperCollider server. `Size` sets the
output length and `Root` sets the MIDI base note (default 60).
For example, `[0, 4, 7]` with Size 8 produces
`[60, 64, 67, 72, 76, 79, 84, 88]`.

`Step`, `Degree Shift`, `Octave Jump` and `Repeat After` control scale
expansion. `Start Index` is zero-based; Skew, Roundness, PW, Num Waves,
Cycle Shift, Symmetry, Comb, Group Size, Scatter, Seed, Reverse and Pow
shape the indices as in PolyComb. Scale order is preserved, duplicate
offsets are removed, and an empty scale falls back to `[0]`.
`Transpose` and `Fine Tune` shift the final `Pitches` vector, which can be
connected to any MIDI pitch input. A compact bar preview lives inside the
node; hover a bar for its index and MIDI value. Preview dimensions are in
the inspector.
