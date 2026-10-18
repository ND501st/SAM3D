#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "SimCampaignHUD.generated.h"

class USimAirCampaignSubsystem;

/**
 * The air war's campaign screen (docs/air_war/, roadmap A10), between raids. Shows what the player's side knows:
 *   - a map of the theatre: the front and the enemy's entry points, our objects (damaged, wrecked), the towns, the battery
 *     with its reach and its dummies, the raids our warning net saw in the last day;
 *   - the intelligence summary (the campaign's visible journal), the enemy's staff as our side knows it (nicknames,
 *     names from prisoners, habits our analysts have seen), the raids seen, and the battery's posture;
 *   - the waiting raid's warning, the seat to fight it from, and the choice to fight it or let it pass.
 * When the campaign ends, the enemy's whole staff is revealed (AirCampaign::EnemyMind).
 *
 * Keys: Space advance to the next raid; Enter fight the waiting raid; L let it pass; [ ] the seat; Tab or 1-4 the panels;
 * on the Posture panel Up/Down a row, Left/Right change it; a click on the map marks a new position, M moves the battery
 * there; E ends the campaign; Q or Esc back to the missions menu. Mouse: the panels' tabs, the buttons, the posture's
 * arrows, the wheel scrolls the summary.
 *
 * Scripted check: -SimCampaignTest=<folder> photographs the screen (campaign.png), advances to a raid and photographs it
 * (campaign_raid.png), then quits.
 */
UCLASS()
class SAM3D_API ASimCampaignHUD : public AHUD
{
	GENERATED_BODY()

public:
	ASimCampaignHUD();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void DrawHUD() override;

private:
	enum class EPanel : uint8
	{
		Intsum,
		Staff,
		Posture,
		Raids
	};

	USimAirCampaignSubsystem* Campaign() const;
	void HandleInput();
	void Advance();
	void Fight();
	void ChangePosture(int32 Row, int32 Step);
	void RunScriptedCheck(float DeltaSeconds);

	float DrawWrapped(const FString& Text, float X, float Y, float Width, UFont* Font, const FLinearColor& Colour, float LineGap = 2.0f,
		float Bottom = 1e9f);
	bool Button(const FString& Label, float X, float Y, float W, float H, bool bPrimary);

	void DrawMap(float X, float Y, float W, float H);
	float DrawPanel(float X, float Y, float W, float H);

	/** Map: theatre metres -> screen and back. */
	FVector2D ToScreen(double EastM, double NorthM) const;
	FVector2D ToTheatre(const FVector2D& Screen) const;
	/** A line on the map, clipped to its drawing area (MapBox); false if none of it is inside. */
	bool MapLine(FVector2D A, FVector2D B, const FLinearColor& Colour, float Thickness);

	EPanel Panel = EPanel::Intsum;
	int32 PostureRow = 0;
	int32 SeatIndex = 0;
	int32 Scroll = 0;
	bool bHasMark = false;
	FVector2D MarkKm = FVector2D::ZeroVector;
	FString Message;

	/** This frame's clickable areas (filled while drawing, read by the next input). */
	struct FClick
	{
		FBox2D Box;
		FString Id;
	};
	TArray<FClick> Clicks;
	TArray<FClick> PendingClicks;
	FBox2D MapBox = FBox2D(ForceInit);  // the map's drawing area (clicks and clipping); the hint is drawn under it
	FVector2D MapMin = FVector2D::ZeroVector;
	FVector2D MapMax = FVector2D(1.0, 1.0);
	float MapScale = 1.0f;
	FVector2D MapOrigin = FVector2D::ZeroVector;

	FString TestFolder;
	float TestClock = 0.0f;
	int32 TestStep = 0;
};
