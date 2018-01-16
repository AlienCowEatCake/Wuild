/*
 * Copyright (C) 2017 Smirnov Vladimir mapron1@gmail.com
 * Source code licensed under the Apache License, Version 2.0 (the "License");
 * You may not use this file except in compliance with the License.
 * You may obtain a copy of the License at http://www.apache.org/licenses/LICENSE-2.0 or in file COPYING-APACHE-2.0.txt
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.h
 */

#pragma once

#include <CoordinatorClient.h>
#include <ThreadLoop.h>
#include <TimePoint.h>
#include <CommonTypes.h>
#include <RemoteToolClientConfig.h>
#include <ToolInvocation.h>
#include <IInvocationRewriter.h>
#include <IToolInvoker.h>

#include <functional>
#include <atomic>

namespace Wuild
{
class RemoteToolClientImpl;

/**
 * @brief Transforms local tool execution command to remote.
 *
 * Recieves remote tool servers list from Coordinator; then connects to all servers.
 * After reciving new task through InvokeTool() - distributes them to servers.
 */
class RemoteToolClient : public IToolInvoker
{
	friend class RemoteToolClientImpl;
public:

	using Config = RemoteToolClientConfig;
	using RemoteAvailableCallback = std::function<void()>;

public:
	RemoteToolClient(IInvocationRewriter::Ptr invocationRewriter);
	~RemoteToolClient();

	bool SetConfig(const Config & config);

	/// Allow call another invoker when remote execution fails.
	void SetInvokerFallback(IToolInvoker * invokerFallback);

	/// Explicitly add new remote tool server client (used for testing)
	void AddClient(const ToolServerInfo & info, bool start = false);

	int GetFreeRemoteThreads() const;

	void Start(const StringVector & requiredToolIds = StringVector());
	void FinishSession();

	void SetRemoteAvailableCallback(RemoteAvailableCallback callback);

	/// Starts new remote task.
	void InvokeTool(const ToolInvocation & invocation, InvokeCallback callback) override;

	std::string GetSessionInformation() const { return m_sessionInfo.ToString(false, true); }

protected:
	void UpdateSessionInfo(const TaskExecutionInfo& executionResult);
	void AvailableCheck();

	ThreadLoop m_thread;

	std::unique_ptr<RemoteToolClientImpl> m_impl;

	bool m_started = false;
	TimePoint m_start;
	TimePoint m_lastFinish;
	int64_t m_sessionId  = 0;
	int64_t m_taskIndex  = 0;
	ToolServerSessionInfo m_sessionInfo;
	std::mutex m_sessionInfoMutex;
	std::mutex m_availableCheckMutex;

	bool m_remoteIsAvailable = false;
	RemoteAvailableCallback m_remoteAvailableCallback;
	Config m_config;
	IInvocationRewriter::Ptr m_invocationRewriter;
};

}
