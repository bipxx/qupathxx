import qupathxx as q

def wrong(project: q.Project, fingerprint: q.FileFingerprint) -> None:
    project.review_source_verification("image", fingerprint)
    project.apply_source_review(fingerprint)
    project.clear_source_verification("image")
