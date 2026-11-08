#include "../gui_spatial.hpp"
#include "../gui_tma.hpp"
#include "../gui_object_classifier.hpp"
#include "../gui_pixel_classifier.hpp"
#include <iostream>
#include <stdexcept>

int main() {
    try {
        using namespace qpx;using namespace qpx::detail;
        auto check=[](bool value) {if(!value) throw std::runtime_error("GUI domain target routing differs");};
        check(GuiSpatial::handles(GuiInputTarget::SpatialOpen));
        check(GuiSpatial::handles(GuiInputTarget::SpatialRecordId));
        check(GuiSpatial::handles(GuiInputTarget::SpatialTissue));
        check(GuiSpatial::handles(GuiInputTarget::SpatialExclusions));
        check(!GuiSpatial::handles(GuiInputTarget::ApplySourceVerification));
        check(GuiTma::handles(GuiInputTarget::CreateTmaGrid));
        check(GuiTma::handles(GuiInputTarget::ApplyTmaLabels));
        check(GuiTma::handles(GuiInputTarget::LoadQptmaPreview));
        check(GuiTma::handles(GuiInputTarget::AcknowledgeQptmaLosses));
        check(GuiTma::handles(GuiInputTarget::TmaDetectGrid));
        check(!GuiTma::handles(GuiInputTarget::OverviewBudgetIncrease));
        check(!GuiTma::handles(GuiInputTarget::RawPixelTrain));
        check(GuiObjectClassifier::handles(GuiInputTarget::TrainObjectClassifier));
        check(GuiObjectClassifier::handles(GuiInputTarget::UseClassifierRecord));
        check(!GuiObjectClassifier::handles(GuiInputTarget::TrainPixelClassifier));
        check(GuiPixelClassifier::handles(GuiInputTarget::TrainPixelClassifier));
        check(GuiPixelClassifier::handles(GuiInputTarget::OpenPixelStreamBrowser));
        check(GuiPixelClassifier::handles(GuiInputTarget::OpenPixelStreamFile));
        check(GuiPixelClassifier::handles(GuiInputTarget::RemovePixelStreamRecord));
        check(!GuiPixelClassifier::handles(GuiInputTarget::CreateTmaGrid));
        check(!GuiPixelStreamBrowser::handles(GuiInputTarget::OpenPixelStreamBrowser));
        check(GuiPixelStreamBrowser::handles(GuiInputTarget::OpenPixelStreamFile));
        check(GuiPixelStreamBrowser::handles(GuiInputTarget::RemovePixelStreamRecord));
        for(auto target:{GuiInputTarget::Canvas,static_cast<GuiInputTarget>(-1),static_cast<GuiInputTarget>(std::numeric_limits<int>::max())}) {
            check(!GuiSpatial::handles(target) && !GuiTma::handles(target) &&
                !GuiObjectClassifier::handles(target) && !GuiPixelClassifier::handles(target) && !GuiPixelStreamBrowser::handles(target));
        }
        std::cout<<"GUI domain membership routing passed\n";
    }catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
