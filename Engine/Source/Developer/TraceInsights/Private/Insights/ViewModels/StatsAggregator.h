// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Async/AsyncWork.h"
#include "Insights/Common/Stopwatch.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

namespace Trace
{
	class IAnalysisSession;
}

namespace Insights
{

////////////////////////////////////////////////////////////////////////////////////////////////////

class IStatsAggregator
{
public:
	virtual void Start() = 0;
	virtual void Cancel() = 0;

	virtual bool IsCancelRequested() const = 0;
	virtual bool IsRunning() const = 0;

	virtual double GetAllOperationsDuration() = 0;
	virtual double GetCurrentOperationDuration() = 0;
	virtual uint32 GetOperationCount() const = 0;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

class IStatsAggregationWorker
{
public:
	virtual ~IStatsAggregationWorker() {}
	virtual void DoWork() = 0;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

class FStatsAggregationTask;
typedef FAsyncTask<FStatsAggregationTask> FStatsAggregationAsyncTask;

////////////////////////////////////////////////////////////////////////////////////////////////////

class FStatsAggregator : public IStatsAggregator
{
public:
	explicit FStatsAggregator(const FString InLogName);
	virtual ~FStatsAggregator();

	double GetIntervalStartTime() const { return IntervalStartTime; }
	double GetIntervalEndTime() const { return IntervalEndTime; }

	void SetTimeInterval(double InStartTime, double InEndTime)
	{
		IntervalStartTime = InStartTime;
		IntervalEndTime = InEndTime;
	}

	void Tick(TSharedPtr<const Trace::IAnalysisSession> InSession, const double InCurrentTime, const float InDeltaTime, TFunctionRef<void()> OnFinishedCallback);

	//////////////////////////////////////////////////
	// IStatsAggregator

	virtual void Start() override;
	virtual void Cancel() override { bIsCancelRequested = true; }

	virtual bool IsCancelRequested() const override { return bIsCancelRequested; }
	virtual bool IsRunning() const override { return AsyncTask != nullptr; }

	virtual double GetAllOperationsDuration() override { AllOpsStopwatch.Update(); return AllOpsStopwatch.GetAccumulatedTime(); }
	virtual double GetCurrentOperationDuration() override { CurrentOpStopwatch.Update(); return CurrentOpStopwatch.GetAccumulatedTime(); }
	virtual uint32 GetOperationCount() const override { return OperationCount; }

	//////////////////////////////////////////////////

protected:
	virtual IStatsAggregationWorker* CreateWorker(TSharedPtr<const Trace::IAnalysisSession> InSession) = 0;

	// Returns true only when it is called from OnFinishedCallback.
	bool IsFinished() const { return bIsFinished; }

	// Gets the worker object. It can only be called from OnFinishedCallback.
	IStatsAggregationWorker* GetWorker() const;

private:
	void ResetAsyncTask();

private:
	FString LogName;

	double IntervalStartTime;
	double IntervalEndTime;

	FStatsAggregationAsyncTask* AsyncTask;

	mutable volatile bool bIsCancelRequested; // true if we want the async task to finish asap
	bool bIsStartRequested;
	bool bIsFinished;

	mutable FStopwatch AllOpsStopwatch;
	mutable FStopwatch CurrentOpStopwatch;
	uint32 OperationCount;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace Insights
