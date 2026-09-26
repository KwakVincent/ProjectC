// Copyright Epic Games, Inc. All Rights Reserved.

#include "BattleRobotCharacter.h"
#include "Engine/LocalPlayer.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "BattleRobot.h"
#include "Voice/VKVoiceCommanderComponent.h"

ABattleRobotCharacter::ABattleRobotCharacter()
{
	VoiceCommander = CreateDefaultSubobject<UVKVoiceCommanderComponent>(TEXT("VoiceCommander"));

	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);
		
	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);

	// Note: For faster iteration times these variables, and many more, can be tweaked in the Character Blueprint
	// instead of recompiling to adjust them
	GetCharacterMovement()->JumpZVelocity = 500.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f;
	CameraBoom->bUsePawnControlRotation = true;

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named ThirdPersonCharacter (to avoid direct content references in C++)
}

void ABattleRobotCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent)) {
		
		// Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ABattleRobotCharacter::Move);
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Completed, this, &ABattleRobotCharacter::MoveEnd);
		EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &ABattleRobotCharacter::Look);

		// Looking
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &ABattleRobotCharacter::Look);
	}
	else
	{
		UE_LOG(LogBattleRobot, Error, TEXT("'%s' Failed to find an Enhanced Input component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}

void ABattleRobotCharacter::Move(const FInputActionValue& Value)
{
	FVector2D const MovementVector = Value.Get<FVector2D>();
	DoMove(MovementVector.X, MovementVector.Y);
}

void ABattleRobotCharacter::MoveEnd(const FInputActionValue& Value)
{
	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;
}

void ABattleRobotCharacter::Look(const FInputActionValue& Value)
{
	FVector2D const LookAxisVector = Value.Get<FVector2D>();
	DoLook(LookAxisVector.X, LookAxisVector.Y);
}

void ABattleRobotCharacter::DoMove(float Right, float Forward)
{
	AController const* CurrentController = GetController();
	if (CurrentController == nullptr)
	{
		return;
	}

	bool const bIsStrafeOrBack = (Forward < -0.1f || FMath::Abs(Right) > 0.1f);
	if (bIsStrafeOrBack)
	{
		bUseControllerRotationYaw = true;
		GetCharacterMovement()->bOrientRotationToMovement = false;
	}
	else
	{
		bUseControllerRotationYaw = false;
		GetCharacterMovement()->bOrientRotationToMovement = true;
	}

	FRotator const Rotation = CurrentController->GetControlRotation();
	FRotator const YawRotation(0.0f, Rotation.Yaw, 0.0f);

	FVector const ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	FVector const RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	AddMovementInput(ForwardDirection, Forward);
	AddMovementInput(RightDirection, Right);
}

void ABattleRobotCharacter::DoLook(float Yaw, float Pitch)
{
	if (GetController() != nullptr)
	{
		// add yaw and pitch input to controller
		AddControllerYawInput(Yaw);
		AddControllerPitchInput(Pitch);
	}
}

void ABattleRobotCharacter::DoJumpStart()
{
	// signal the character to jump
	Jump();
}

void ABattleRobotCharacter::DoJumpEnd()
{
	// signal the character to stop jumping
	StopJumping();
}

void ABattleRobotCharacter::GodMode()
{
	if (VoiceCommander != nullptr)
	{
		VoiceCommander->SetConsecutiveActionLevel(10);
		if (GEngine != nullptr)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Cyan, TEXT("★ [치트 발동] GodMode 활성화! 연속 명령 10단계 최종 해금!"));
		}
		UE_LOG(LogTemp, Warning, TEXT("★ [치트 발동] GodMode 활성화! 연속 명령 10단계 최종 해금! (최대 10연속 액션 큐잉 가능)"));
	}
}

void ABattleRobotCharacter::SetStrafeMode(bool const bEnable)
{
	UCharacterMovementComponent* MoveComp = GetCharacterMovement();
	if (MoveComp == nullptr)
	{
		return;
	}

	mbIsStrafing = bEnable;
	mbIsBackpedaling = bEnable;

	if (bEnable)
	{
		MoveComp->bOrientRotationToMovement = false;

		AController const* CurrentController = GetController();
		if (CurrentController != nullptr)
		{
			FRotator const ControlRot = CurrentController->GetControlRotation();
			SetActorRotation(FRotator(0.0f, ControlRot.Yaw, 0.0f));
		}
	}
	else
	{
		MoveComp->bOrientRotationToMovement = true;
	}
}

bool ABattleRobotCharacter::IsStrafing() const
{
	return mbIsStrafing;
}

void ABattleRobotCharacter::SetBackpedalMode(bool const bEnable)
{
	SetStrafeMode(bEnable);
}

bool ABattleRobotCharacter::IsBackpedaling() const
{
	return mbIsBackpedaling;
}

float ABattleRobotCharacter::GetMovementDirection() const
{
	FVector const Velocity = GetVelocity();
	if (Velocity.IsNearlyZero())
	{
		return 0.0f;
	}

	FRotator BaseRot = GetActorRotation();
	AController const* CurrentController = GetController();
	if (CurrentController != nullptr)
	{
		BaseRot = FRotator(0.0f, CurrentController->GetControlRotation().Yaw, 0.0f);
	}

	FMatrix const RotMatrix = FRotationMatrix(BaseRot);
	FVector const ForwardVector = RotMatrix.GetScaledAxis(EAxis::X);
	FVector const RightVector = RotMatrix.GetScaledAxis(EAxis::Y);
	FVector const NormalizedVelocity = Velocity.GetSafeNormal2D();

	float const ForwardDot = FVector::DotProduct(ForwardVector, NormalizedVelocity);
	float const RightDot = FVector::DotProduct(RightVector, NormalizedVelocity);

	float const AngleRadians = FMath::Atan2(RightDot, ForwardDot);
	float const CalculatedDegrees = FMath::RadiansToDegrees(AngleRadians);

	return CalculatedDegrees;
}
