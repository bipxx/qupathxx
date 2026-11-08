import qupathxx as q

def checked(project: q.Project, image: int, fingerprint: q.FileFingerprint) -> q.SourceFileAssociation | None:
    review: q.ProjectSourceReview = project.review_source_verification(image, fingerprint)
    project.apply_source_review(review, accept_changed=False)
    return project.source_verification(image)
