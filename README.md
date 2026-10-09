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
