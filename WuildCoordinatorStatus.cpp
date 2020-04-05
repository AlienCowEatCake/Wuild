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

#include "AppUtils.h"

#include <CoordinatorClient.h>
#include <RemoteToolFrames.h>
#include <SocketFrameHandler.h>
#include <StringUtils.h>
#include <TimePoint.h>

#include <iostream>
#include <set>

namespace Wuild {

class RemoteToolVersionClient
{
public:
	using Config = RemoteToolClientConfig;
	using VersionInfoArrivedCallback = std::function<void(const std::map<std::string, std::string>&, const ToolServerInfo&)>;

public:
	RemoteToolVersionClient() = default;
	~RemoteToolVersionClient() = default;

	bool SetConfig(const Config & config)
	{
		std::ostringstream os;
		if (!config.Validate(&os))
		{
			Syslogger(Syslogger::Err) << os.str();
			return false;
		}
		m_config = config;
		return true;
	}

	void AddClient(const ToolServerInfo & info)
	{
		const std::lock_guard<std::mutex> lock(m_clientsMutex);
		SocketFrameHandlerSettings settings;
		settings.m_channelProtocolVersion = RemoteToolRequest::s_version + RemoteToolResponse::s_version;
		settings.m_segmentSize = 8192;
		settings.m_hasConnStatus = true;
		SocketFrameHandler::Ptr handler(new SocketFrameHandler(settings));
		handler->RegisterFrameReader(SocketFrameReaderTemplate<ToolsVersionResponse>::Create());
		handler->SetTcpChannel(info.m_connectionHost, info.m_connectionPort);

		auto versionFrameCallback = [this, info](SocketFrame::Ptr responseFrame, SocketFrameHandler::ReplyState state, const std::string & errorInfo)
		{
			if (state == SocketFrameHandler::ReplyState::Timeout || state == SocketFrameHandler::ReplyState::Error)
			{
				Syslogger(Syslogger::Err) << "Error on requesting toolserver " << info.m_connectionHost << ", " << errorInfo;
			}
			else if (m_versionInfoArrivedCallback)
			{
				ToolsVersionResponse::Ptr result = std::dynamic_pointer_cast<ToolsVersionResponse>(responseFrame);
				m_versionInfoArrivedCallback(result->m_versions, info);
			}
		};

		handler->QueueFrame(ToolsVersionRequest::Ptr(new ToolsVersionRequest()), versionFrameCallback, m_config.m_requestTimeout);
		m_clients.push_back(handler);
		handler->Start();
	}

	void SetVersionInfoArrivedCallback(VersionInfoArrivedCallback callback)
	{
		m_versionInfoArrivedCallback = std::move(callback);
	}

private:
	std::mutex m_clientsMutex;
	std::deque<SocketFrameHandler::Ptr> m_clients;
	VersionInfoArrivedCallback m_versionInfoArrivedCallback;
	Config m_config;
};

}


int main(int argc, char** argv)
{
	using namespace Wuild;
	ConfiguredApplication app(argc, argv, "WuildProxyClient", "proxy");

	CoordinatorClient::Config config = app.m_remoteToolClientConfig.m_coordinator;
	if (!config.Validate())
		return 1;

	CoordinatorClient client;
	if (!client.SetConfig(config))
		return 1;

	RemoteToolVersionClient toolVersionClient;
	const bool toolVersionClientConfigured = toolVersionClient.SetConfig(app.m_remoteToolClientConfig);
	std::mutex toolVersionClientLogMutex;
	std::atomic_size_t toolVersionClientWaitCounter{0};

	client.SetInfoArrivedCallback([&](const CoordinatorInfo& info){
		std::cout << "Coordinator connected tool servers: \n";

		std::map<int64_t, int> sessionsUsed;
		std::set<std::string> toolIds;
		int running = 0, thread = 0, queued = 0;

		for (const ToolServerInfo & toolServer : info.m_toolServers)
		{
			std::cout <<  toolServer.m_connectionHost << ":" << toolServer.m_connectionPort <<
						  " load:" << toolServer.m_runningTasks << "/" << toolServer.m_totalThreads << "\n";
			running += toolServer.m_runningTasks;
			queued += toolServer.m_queuedTasks;
			thread += toolServer.m_totalThreads;
			for (const ToolServerInfo::ConnectedClientInfo & client : toolServer.m_connectedClients)
			{
				sessionsUsed[client.m_sessionId] += client.m_usedThreads ;
			}
			for (const std::string & t : toolServer.m_toolIds)
				toolIds.insert(t);
		}

		std::cout <<  "\nTotal load:" << running << "/" << thread << ", queue: " << queued << "\n";

		if (!sessionsUsed.empty())
		{
			std::cout << "\nRunning sessions:\n";
			for (const auto & s : sessionsUsed)
			{
				TimePoint stamp; stamp.SetUS(s.first); stamp.ToLocal();
				std::cout << "Started " << stamp.ToString() << ", used: " << s.second << "\n";
			}
		}

		std::cout << "\nAvailable tools: ";
		for (const auto & t : toolIds)
			std::cout << t << ", ";
		std::cout << "\n";

		std::flush(std::cout);
		if (toolVersionClientConfigured)
		{
			for (const ToolServerInfo & toolServer : info.m_toolServers)
			{
				++toolVersionClientWaitCounter;
				toolVersionClient.AddClient(toolServer);
			}
		}

		if (!toolVersionClientConfigured || info.m_toolServers.empty())
			Application::Interrupt(0);
	});

	toolVersionClient.SetVersionInfoArrivedCallback([&](const std::map<std::string, std::string> & versionMap, const ToolServerInfo & info){
		const std::lock_guard<std::mutex> lock(toolVersionClientLogMutex);

		std::cout << "\n" << info.m_connectionHost << ":" << info.m_connectionPort << ":\n";
		for (const auto & versionInfo : versionMap)
			std::cout << "  " << versionInfo.first << ", version = \"" << versionInfo.second << "\"\n";
		std::flush(std::cout);

		if (--toolVersionClientWaitCounter == 0)
			Application::Interrupt(0);
	});

	client.Start();

	return ExecAppLoop();
}
