from qupathxx import StainTransform, GuiOptions, GuiReport, stain_model_json, stain_model_from_json, save_stain_model, load_stain_model

def use(model: StainTransform, report: GuiReport) -> None:
    text: str = stain_model_json(model)
    loaded: StainTransform = stain_model_from_json(text)
    save_stain_model(loaded, "model.json")
    model = load_stain_model("model.json")
    options = GuiOptions()
    options.stain_model_path = "model.json"
    state: str = report.stain_model_state
    error: str = report.stain_model_error
    digest: str = report.stain_model_sha256
