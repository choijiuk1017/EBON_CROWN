#if WITH_EDITOR
#include "InlineInputPrompts.h"
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/WidgetSwitcher.h"
#include "Engine/Texture2D.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#include "UObject/MetaData.h"

namespace
{
const FString Root = TEXT("/Game/BluePrint/UI_Widget/");
template<class T>
T* Make(UWidgetTree* Tree, const FString& Name)
{
    T* Widget = Tree->ConstructWidget<T>(T::StaticClass(), FName(*Name));
    Widget->bIsVariable = false;
    return Widget;
}

void SetIcon(UImage* Image, const FString& Key, float Size)
{
    UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, *(TEXT("/Game/UI/InputPrompts/") + Key + TEXT(".") + Key));
    Image->SetBrushFromTexture(Texture);
    Image->SetDesiredSizeOverride(FVector2D(Size));
    Image->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void AddIcon(UHorizontalBox* Row, UWidgetTree* Tree, const FString& Name, const FString& Key, float Size)
{
    UImage* Image = Make<UImage>(Tree, Name);
    SetIcon(Image, Key, Size);
    UHorizontalBoxSlot* Slot = Row->AddChildToHorizontalBox(Image);
    Slot->SetVerticalAlignment(VAlign_Center);
    Slot->SetPadding(FMargin(0, 0, 5, 0));
}

void SaveWidget(UWidgetBlueprint* BP)
{
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
    FKismetEditorUtilities::CompileBlueprint(BP);
    if (BP->Status == BS_Error)
    {
        UE_LOG(LogTemp, Error, TEXT("Input prompt blueprint failed to compile: %s"), *BP->GetPathName());
        return;
    }
    UPackage* Package = BP->GetOutermost();
    const FString File = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
    FSavePackageArgs Args;
    Args.TopLevelFlags = RF_Public | RF_Standalone;
    Package->GetMetaData()->SetValue(BP, TEXT("EbonDesignerPrompts"), TEXT("1"));
    if (!UPackage::SavePackage(Package, BP, *File, Args))
        UE_LOG(LogTemp, Error, TEXT("Could not save input prompt blueprint %s"), *File);
}

void AddLabel(UHorizontalBox* Row, UWidgetTree* Tree, const FString& Prefix, const TCHAR* Key, const TCHAR* Label, bool bBack = false)
{
    AddIcon(Row, Tree, (bBack ? TEXT("BackIcon") : TEXT("Icon")) + Prefix, Key, 32);
    UTextBlock* Text = Make<UTextBlock>(Tree, TEXT("Label") + Prefix);
    Text->SetText(FText::FromString(Label));
    auto Font = Text->GetFont();
    Font.Size = 13;
    Text->SetFont(Font);
    UHorizontalBoxSlot* Slot = Row->AddChildToHorizontalBox(Text);
    Slot->SetVerticalAlignment(VAlign_Center);
    Slot->SetPadding(FMargin(0, 0, 10, 0));
}

void AddMenuGuide(const TCHAR* Asset, const TCHAR* Select, const TCHAR* BackKey, const TCHAR* Back)
{
    const FString Path = Root + Asset;
    UWidgetBlueprint* BP = LoadObject<UWidgetBlueprint>(nullptr, *(Path + TEXT(".") + FPackageName::GetShortName(Path)));
    if (!BP || !BP->WidgetTree || BP->GetOutermost()->IsDirty()) return;
    UWidgetTree* Tree = BP->WidgetTree;
    if (!Tree->RootWidget || Tree->FindWidget(TEXT("MenuInputGuide"))) return;
    UWidget* Content = Tree->RootWidget;
    UOverlay* Layout = Make<UOverlay>(Tree, TEXT("MenuGuideLayout"));
    Tree->RootWidget = Layout;
    UOverlaySlot* Main = Layout->AddChildToOverlay(Content);
    Main->SetHorizontalAlignment(HAlign_Fill);
    Main->SetVerticalAlignment(VAlign_Fill);
    UHorizontalBox* Row = Make<UHorizontalBox>(Tree, TEXT("MenuInputGuide"));
    Row->SetVisibility(ESlateVisibility::HitTestInvisible);
    UOverlaySlot* Footer = Layout->AddChildToOverlay(Row);
    Footer->SetHorizontalAlignment(HAlign_Right);
    Footer->SetVerticalAlignment(VAlign_Bottom);
    Footer->SetPadding(FMargin(24));
    AddLabel(Row, Tree, TEXT("Navigate"), TEXT("keyboard_arrows"), Select);
    AddLabel(Row, Tree, TEXT("Confirm"), TEXT("keyboard_enter"), TEXT("결정"));
    AddLabel(Row, Tree, TEXT("Back"), BackKey, Back, true);
    SaveWidget(BP);
}

void InstallCreditsText()
{
    UWidgetBlueprint* BP = LoadObject<UWidgetBlueprint>(nullptr, TEXT("/Game/BluePrint/UI_Widget/WBP_Title.WBP_Title"));
    if (!BP || !BP->WidgetTree || BP->GetOutermost()->IsDirty()) return;
    UWidgetTree* Tree = BP->WidgetTree;
    if (Tree->FindWidget(TEXT("CreditsText"))) return;
    UButton* Old = Cast<UButton>(Tree->FindWidget(TEXT("Button_0")));
    UTextBlock* Exit = Cast<UTextBlock>(Tree->FindWidget(TEXT("ExitText")));
    if (!Old || !Old->GetParent() || !Exit) return;
    // Retain the animated button as a collapsed binding target in the existing blueprint.
    UOverlay* Layout = Make<UOverlay>(Tree, TEXT("CreditEntryLayout"));
    if (!Old->GetParent()->ReplaceChild(Old, Layout)) return;
    Old->Slot = nullptr;
    Layout->AddChildToOverlay(Old);
    Old->SetVisibility(ESlateVisibility::Collapsed);
    UTextBlock* Credits = Make<UTextBlock>(Tree, TEXT("CreditsText"));
    Credits->bIsVariable = true;
    UTextBlock* Caption = Cast<UTextBlock>(Old->GetContent());
    Credits->SetText(Caption ? Caption->GetText() : FText::FromString(TEXT("Credit")));
    Credits->SetFont(Exit->GetFont());
    Credits->SetShadowColorAndOpacity(Exit->GetShadowColorAndOpacity());
    Credits->SetShadowOffset(Exit->GetShadowOffset());
    UOverlaySlot* Slot = Layout->AddChildToOverlay(Credits);
    Slot->SetHorizontalAlignment(HAlign_Center);
    Slot->SetVerticalAlignment(VAlign_Center);
    Credits->SetVisibility(ESlateVisibility::Visible);
    SaveWidget(BP);
}

void CreateOverlay(bool bSkip)
{
    const FString Name = bSkip ? TEXT("WBP_CutsceneSkip") : TEXT("WBP_InputHints");
    const FString Path = Root + TEXT("Input/") + Name;
    if (FPackageName::DoesPackageExist(Path)) return;
    UPackage* Package = CreatePackage(*Path);
    UWidgetBlueprint* BP = CastChecked<UWidgetBlueprint>(FKismetEditorUtilities::CreateBlueprint(
        UUserWidget::StaticClass(), Package, FName(*Name), BPTYPE_Normal, UWidgetBlueprint::StaticClass(), UWidgetBlueprintGeneratedClass::StaticClass()));
    UWidgetTree* Tree = BP->WidgetTree;
    UOverlay* RootPanel = Make<UOverlay>(Tree, TEXT("Layout"));
    Tree->RootWidget = RootPanel;
    UWidgetSwitcher* Pages = Make<UWidgetSwitcher>(Tree, TEXT("HintPages"));
    UOverlaySlot* Corner = RootPanel->AddChildToOverlay(Pages);
    Corner->SetHorizontalAlignment(HAlign_Right);
    Corner->SetVerticalAlignment(bSkip ? VAlign_Bottom : VAlign_Top);
    Corner->SetPadding(FMargin(20));
    for (int32 Page = 0; Page < (bSkip ? 1 : 6); ++Page)
    {
        UHorizontalBox* Row = Tree->ConstructWidget<UHorizontalBox>();
        Pages->AddChild(Row);
        auto Add = [&](const TCHAR* Key, const TCHAR* Label, bool bBack = false)
        {
            AddLabel(Row, Tree, FString::Printf(TEXT("_%d_%s"), Page, Key), Key, Label, bBack);
        };
        if (bSkip) Add(TEXT("keyboard_space"), TEXT("건너뛰기"));
        else if (Page == 0)
        {
            Add(TEXT("keyboard_f"), TEXT("상호작용")); Add(TEXT("mouse_left"), TEXT("공격"));
            Add(TEXT("keyboard_q"), TEXT("스킬")); Add(TEXT("keyboard_c"), TEXT("구르기"));
            Add(TEXT("keyboard_x"), TEXT("포션")); Add(TEXT("keyboard_z"), TEXT("포션 변경"));
            Add(TEXT("keyboard_e"), TEXT("인벤토리"));
        }
        else
        {
            if (Page != 1) Add(TEXT("keyboard_arrows"), TEXT("선택"));
            Add(TEXT("keyboard_enter"), Page == 2 ? TEXT("장착") : TEXT("확인"));
            if (Page == 2)
            {
                Add(TEXT("keyboard_delete"), TEXT("해제")); Add(TEXT("keyboard_page_up"), TEXT("이전 탭"));
                Add(TEXT("keyboard_page_down"), TEXT("다음 탭"));
            }
            if (Page != 1 && Page != 3)
                Add(Page == 2 ? TEXT("keyboard_e") : Page == 4 ? TEXT("keyboard_escape") : TEXT("keyboard_x"), TEXT("닫기"), true);
        }
    }
    FAssetRegistryModule::AssetCreated(BP);
    SaveWidget(BP);
}

// Only migrate the existing input instructions. Keep the original TextBlock and its bindings.
bool MigrateLabel(UWidgetBlueprint* BP, UTextBlock* Text)
{
    UWidgetTree* Tree = BP->WidgetTree;
    const FString Name = BP->GetName();
    const FString Source = Text->GetText().ToString().TrimStartAndEnd();
    FString Key, Label = Source;
    if (Name == TEXT("WBP_Title"))
    {
        if (Text->GetFName() == TEXT("TextBlock")) { Key = TEXT("keyboard_arrows"); Label = TEXT("변경"); }
        if (Text->GetFName() == TEXT("TextBlock_1")) { Key = TEXT("keyboard_enter"); Label = TEXT("확인"); }
    }
    else if (Name.StartsWith(TEXT("WBP_Tutorial_")))
    {
        if (Source.Contains(TEXT("방향키"))) { Key = TEXT("keyboard_arrows"); Label = TEXT("변경"); }
        else if (Source.Contains(TEXT("엔터로"))) { Key = TEXT("keyboard_enter"); Label = TEXT("확인"); }
        // These diagrams already include separate key images.
        else if (Source.Contains(TEXT("WASD"))) Label = TEXT("이동");
        else if (Source.Contains(TEXT("마우스를"))) Label = TEXT("시점 조절");
        else if (Source.StartsWith(TEXT("X 키"))) Label = TEXT("사용");
        else if (Source.StartsWith(TEXT("Z 키"))) Label = TEXT("변경");
    }
    else if (Source == TEXT("나가기") || Source == TEXT("닫기"))
        Key = Name == TEXT("WBP_InventoryWidget") ? TEXT("keyboard_e") : TEXT("keyboard_x");
    else if (Source == TEXT("변경") || Source == TEXT("무기 선택")) Key = TEXT("keyboard_arrows");
    else if (Source == TEXT("결정") || Source == TEXT("강화") || Source == TEXT("확인"))
        Key = Name == TEXT("WBP_Dialogue") ? TEXT("keyboard_f") : TEXT("keyboard_enter");

    const FString IconName = TEXT("InputPrompt_") + Text->GetName();
    bool Changed = Label != Source;
    if (Changed) Text->SetText(FText::FromString(Label));
    if (Key.IsEmpty() || Tree->FindWidget(FName(*IconName))) return Changed;
    UPanelWidget* Parent = Text->GetParent();
    if (!Parent) return Changed;
    UHorizontalBox* Row = Make<UHorizontalBox>(Tree, TEXT("PromptRow_") + Text->GetName());
    if (!Parent->ReplaceChild(Text, Row)) return Changed;
    Text->Slot = nullptr;
    AddIcon(Row, Tree, IconName, Key, FMath::Clamp(Text->GetFont().Size * 1.65f, 32.f, 96.f));
    Row->AddChildToHorizontalBox(Text)->SetVerticalAlignment(VAlign_Center);
    return true;
}

bool MigrateBorder(UWidgetTree* Tree, UBorder* Border)
{
    UObject* Resource = Border->Background.GetResourceObject();
    if (!Resource || (!Resource->GetPathName().StartsWith(TEXT("/Game/ImageResource/UI/Key/")) &&
        !Resource->GetPathName().StartsWith(TEXT("/Game/UI/InputPrompts/")))) return false;
    UOverlay* Layers = Make<UOverlay>(Tree, TEXT("IconLayers_") + Border->GetName());
    if (UPanelWidget* Parent = Border->GetParent())
    {
        if (!Parent->ReplaceChild(Border, Layers)) return false;
        Border->Slot = nullptr;
    }
    else if (Tree->RootWidget == Border) Tree->RootWidget = Layers;
    else return false;
    UImage* Image = Make<UImage>(Tree, TEXT("PromptImage_") + Border->GetName());
    Image->SetBrush(Border->Background);
    Image->SetVisibility(ESlateVisibility::HitTestInvisible);
    for (UWidget* Layer : {static_cast<UWidget*>(Image), static_cast<UWidget*>(Border)})
    {
        UOverlaySlot* Slot = Layers->AddChildToOverlay(Layer);
        Slot->SetHorizontalAlignment(HAlign_Fill);
        Slot->SetVerticalAlignment(VAlign_Fill);
    }
    FSlateBrush Empty;
    Empty.DrawAs = ESlateBrushDrawType::NoDrawType;
    Border->SetBrush(Empty);
    return true;
}
}

void InstallDesignerInputPrompts()
{
    for (const TCHAR* Asset : {TEXT("WBP_Interaction"), TEXT("WBP_Title"), TEXT("NPC/WBP_Dialogue"),
        TEXT("NPC/WBP_Weapon_Updrade"), TEXT("Inventory/WBP_InventoryWidget"),
        TEXT("SavePointMenu/WBP_LevelUpMenu"), TEXT("SavePointMenu/WBP_WorldTravelMenu"),
        TEXT("Tutorial/WBP_Tutorial_Battle"), TEXT("Tutorial/WBP_Tutorial_Equipment"),
        TEXT("Tutorial/WBP_Tutorial_Movement"), TEXT("Tutorial/WBP_Tutorial_SavePoint"),
        TEXT("Tutorial/WBP_Tutorial_SavePoint_Menu"), TEXT("Tutorial/WBP_Tutorial_Skill"), TEXT("Tutorial/WBP_Tutorial_UsePotion")})
    {
        FString Path = Root + Asset;
        UWidgetBlueprint* BP = LoadObject<UWidgetBlueprint>(nullptr, *(Path + TEXT(".") + FPackageName::GetShortName(Path)));
        if (!BP || !BP->WidgetTree || BP->GetOutermost()->IsDirty()) continue;
        if (BP->GetOutermost()->GetMetaData()->HasValue(BP, TEXT("EbonDesignerPrompts"))) continue;
        TArray<UWidget*> Widgets;
        BP->WidgetTree->GetAllWidgets(Widgets);
        bool Changed = false;
        for (UWidget* Widget : Widgets)
        {
            if (UTextBlock* Text = Cast<UTextBlock>(Widget)) Changed |= MigrateLabel(BP, Text);
            else if (UBorder* Border = Cast<UBorder>(Widget)) Changed |= MigrateBorder(BP->WidgetTree, Border);
        }
        if (Changed) SaveWidget(BP);
    }
    InstallCreditsText();
    AddMenuGuide(TEXT("SavePointMenu/WBP_SavePointMenu"), TEXT("항목 선택"), TEXT("keyboard_x"), TEXT("떠나기"));
    AddMenuGuide(TEXT("NPC/WBP_Dialogue_Choice"), TEXT("대답 선택"), TEXT("keyboard_x"), TEXT("거절"));
    AddMenuGuide(TEXT("WBP_PauseMenu"), TEXT("항목 선택"), TEXT("keyboard_escape"), TEXT("게임으로 복귀"));
    CreateOverlay(false);
    CreateOverlay(true);
}
#endif
