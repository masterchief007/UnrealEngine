// Copyright Epic Games, Inc. All Rights Reserved.

#include "MovieScene/Parameters/MovieSceneNiagaraParameterTrack.h"

const FNiagaraVariable& UMovieSceneNiagaraParameterTrack::GetParameter() const
{
	return Parameter;
}

void UMovieSceneNiagaraParameterTrack::SetParameter(FNiagaraVariable InParameter)
{
	Parameter = InParameter;
}