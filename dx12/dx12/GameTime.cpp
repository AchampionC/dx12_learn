#include "GameTime.h"

GameTime::GameTime() : mSencondsPerCount(0.0), mDeltaTime(-1.0), mBaseTime(0),
mPauseTime(0), mStopTime(0), mPrevTime(0), mCurrentTime(0), isStoped(false)
{
	//计算计数器每秒多少次，并存入countsPerSec中返回
	//注意，此处为QueryPerformanceFrequency函数
	__int64 countsPerSec;
	QueryPerformanceFrequency((LARGE_INTEGER*)&countsPerSec);
	mSencondsPerCount = 1.0 / (double)countsPerSec;
}

void GameTime::Tick()
{
	if (isStoped)
	{
		//如果当前是停止状态，则帧间隔时间为0
		mDeltaTime = 0.0;
		return;
	}
	//计算当前时刻的计数值
	__int64 currentTime;
	QueryPerformanceCounter((LARGE_INTEGER*)&currentTime);
	mCurrentTime = currentTime;
	//计算当前帧和前一帧的时间差(计数差*每次多少秒)
	mDeltaTime = (mCurrentTime - mPrevTime) * mSencondsPerCount;
	//准备计算当前帧和下一帧的时间差
	mPrevTime = mCurrentTime;
	//排除时间差为负值
	if (mDeltaTime < 0.0)
	{
		mDeltaTime = 0.0;
	}
}

float GameTime::DeltaTime()const
{
	return (float)mDeltaTime;
}

void GameTime::Reset()
{
	__int64 currTime;
	QueryPerformanceCounter((LARGE_INTEGER*)&currTime);

	mBaseTime = currTime;	//当前时间作为基准时间
	mPrevTime = currTime;	//当前时间作为上一帧时间，因为重置了，此时没有上一帧
	mStopTime = 0;			//重置停止那一刻时间为0
	isStoped = false;		//重置后的状态为不停止
}

void GameTime::Stop()
{
	if (!isStoped)//如果没有停止，则让其停止（如果停止则什么都不做）
	{
		__int64 currTime;
		QueryPerformanceCounter((LARGE_INTEGER*)&currTime);
		mStopTime = currTime;	//将当前时间作为停止那一刻的时间(次数)
		isStoped = true;	//修改为停止状态
	}
}

void GameTime::Start()
{
	__int64 startTime;
	QueryPerformanceCounter((LARGE_INTEGER*)&startTime);
	if (isStoped)//如果是停止状态，则让其解除停止，保存暂停时间，修改停止状态
	{
		//计算出暂停的总时间（保存停止时间）
		mPauseTime += (startTime - mStopTime);
		//修改停止状态
		mPrevTime = startTime;//相当于重置上一帧时刻
		mStopTime = 0;	//相当于重置停止的时刻
		isStoped = false;	//停止状态为假
	}
	//如果不是停止状态，则什么都不做
}

float GameTime::TotalTime()const
{
	if (isStoped)	//如果此时在暂停状态，则用停止时刻的时间去减之前暂停的总时间
	{
		return (float)((mStopTime - mPauseTime - mBaseTime) * mSencondsPerCount);
	}
	else
	{
		//如果不在暂停状态，则用当前时刻的时间去减暂停总时间
		return (float)((mCurrentTime - mPauseTime - mBaseTime) * mSencondsPerCount);
	}
}

bool GameTime::IsStoped()
{
	return isStoped;
}

