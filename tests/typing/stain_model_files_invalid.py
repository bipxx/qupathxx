from qupathxx import StainTransform, load_stain_model, save_stain_model

def misuse(model: StainTransform) -> None:
    load_stain_model(42)
    save_stain_model("model.json", model)
