// Copyright Epic Games, Inc. All Rights Reserved.

#include "TP_WeaponComponent.h"
#include "ShooterSocketTestCharacter.h"
#include "ShooterSocketTestProjectile.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "WebSocketsModule.h"
#include "JsonUtilities.h"

// Sets default values for this component's properties
UTP_WeaponComponent::UTP_WeaponComponent()
{
    // Default offset from the character location for projectiles to spawn
    MuzzleOffset = FVector(100.0f, 0.0f, 10.0f);
}

void UTP_WeaponComponent::Fire()
{
    if (Character == nullptr || Character->GetController() == nullptr)
    {
        return;
    }

    // Try and fire a projectile
    if (ProjectileClass != nullptr)
    {
        UWorld *const World = GetWorld();
        if (World != nullptr)
        {
            APlayerController *PlayerController = Cast<APlayerController>(Character->GetController());
            const FRotator SpawnRotation = PlayerController->PlayerCameraManager->GetCameraRotation();
            // MuzzleOffset is in camera space, so transform it to world space before offsetting from the character location to find the final muzzle position
            const FVector SpawnLocation = GetOwner()->GetActorLocation() + SpawnRotation.RotateVector(MuzzleOffset);

            // Set Spawn Collision Handling Override
            FActorSpawnParameters ActorSpawnParams;
            ActorSpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;

            // Spawn the projectile at the muzzle
            World->SpawnActor<AShooterSocketTestProjectile>(ProjectileClass, SpawnLocation, SpawnRotation, ActorSpawnParams);
        }
    }

    // Try and play the sound if specified
    if (FireSound != nullptr)
    {
        UGameplayStatics::PlaySoundAtLocation(this, FireSound, Character->GetActorLocation());
    }

    // Try and play a firing animation if specified
    if (FireAnimation != nullptr)
    {
        // Get the animation object for the arms mesh
        UAnimInstance *AnimInstance = Character->GetMesh1P()->GetAnimInstance();
        if (AnimInstance != nullptr)
        {
            AnimInstance->Montage_Play(FireAnimation, 1.f);
        }
    }
}

void UTP_WeaponComponent::AttachWeapon(AShooterSocketTestCharacter *TargetCharacter)
{
    Character = TargetCharacter;

    // Check that the character is valid, and has no rifle yet
    if (Character == nullptr || Character->GetHasRifle())
    {
        return;
    }

    // Attach the weapon to the First Person Character
    FAttachmentTransformRules AttachmentRules(EAttachmentRule::SnapToTarget, true);
    AttachToComponent(Character->GetMesh1P(), AttachmentRules, FName(TEXT("GripPoint")));

    // switch bHasRifle so the animation blueprint can switch to another animation set
    Character->SetHasRifle(true);

    // Set up action bindings
    if (APlayerController *PlayerController = Cast<APlayerController>(Character->GetController()))
    {
        if (UEnhancedInputLocalPlayerSubsystem *Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
        {
            // Set the priority of the mapping to 1, so that it overrides the Jump action with the Fire action when using touch input
            Subsystem->AddMappingContext(FireMappingContext, 1);
        }

        if (UEnhancedInputComponent *EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerController->InputComponent))
        {
            // Fire
            EnhancedInputComponent->BindAction(FireAction, ETriggerEvent::Triggered, this, &UTP_WeaponComponent::Fire);
        }
    }
}

void UTP_WeaponComponent::BeginPlay()
{
    Super::BeginPlay();
    if (!FModuleManager::Get().IsModuleLoaded("WebSockets"))
    {
        FModuleManager::Get().LoadModule("WebSockets");
    }

    Socket = FWebSocketsModule::Get().CreateWebSocket("ws://localhost:3001");

    // event handlers
    Socket->OnConnected().AddLambda([]()
                                    { GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Green, "Successfully Connected"); });

    Socket->OnConnectionError().AddLambda([](const FString &Error)
                                          { GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Red, Error); });

    Socket->OnMessage().AddLambda([this](const FString &MessageString)
                                  {
		// GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Green, MessageString); 
		
		// Initialize a TSharedPtr to hold the output JSON object
		TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject());

		// Deserialize the string into the JSON object
		TSharedRef<TJsonReader<TCHAR>> JsonReader = TJsonReaderFactory<TCHAR>::Create(MessageString);

		if (FJsonSerializer::Deserialize(JsonReader, JsonObject))
		{
			// Successfully converted string to JSON
			// Now you can work with the JsonObject
			FString Value;
			if (JsonObject->TryGetStringField("shoot", Value))
			{
				// Access the value of the "shoot" field
				if (Value == "true") {
					GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Green, Value);
					Fire();
				}
			}
		}
		else
		{
			// Failed to convert string to JSON
			UE_LOG(LogTemp, Error, TEXT("Failed to convert string to JSON"));
		} });

    Socket->Connect();
}

void UTP_WeaponComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (Character == nullptr)
    {
        return;
    }

    if (APlayerController *PlayerController = Cast<APlayerController>(Character->GetController()))
    {
        if (UEnhancedInputLocalPlayerSubsystem *Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
        {
            Subsystem->RemoveMappingContext(FireMappingContext);
        }
    }

    if (Socket->IsConnected())
    {
        Socket->Close();
    }
}

void UTP_WeaponComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // print out the world location
    FVector location = GetComponentLocation();
    // Convert the FVector to a FString for display
    FString LocationString = FString::Printf(TEXT("Gun Location: X=%.2f, Y=%.2f, Z=%.2f"), location.X, location.Y, location.Z);
    // GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Green, LocationString);
}