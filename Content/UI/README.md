# UI widget list

Parent classes live in C++ (`UOCHUDWidget`, `UOCGameUserSettings`). Create the UMG assets in-editor (`docs/08-editor-checklist.md`).

| Widget | Parent | Bound to |
|---|---|---|
| WBP_OCHUD | UOCHUDWidget | Stats, resources, objectives, hints, heatmap, season, music |
| WBP_BuildMenu | UUserWidget | SetBuildMode, PlaceBuilding, wall drag preview |
| WBP_Research | UUserWidget | StartResearch, tech tree from TechTree.csv |
| WBP_Crafting | UUserWidget | QueueRecipe |
| WBP_Military | UUserWidget | Squads, vehicles, airstrike, nuke |
| WBP_Policies | UUserWidget | SetPolicy, DesignateQuarantineZone |
| WBP_International | UUserWidget | Aid, trade, evacuate |
| WBP_Pause | UUserWidget | Save/load, settings, quit |
| WBP_Settings | UUserWidget | UOCGameUserSettings |
| WBP_MainMenu | UUserWidget | UOCGameInstance pending fields |
| WBP_AfterAction | UUserWidget | GenerateAfterActionReport |
| WBP_Lobby | UUserWidget | UOCSteamSubsystem lobbies |
