// Tank 2 Final.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#ifndef _BOTZONE_ONLINE
#include "pch.h"
#endif
// Tank2 游戏样例程序
// 随机策略
// 作者：289371298 upgraded from zhouhy
// https://www.botzone.org.cn/games/Tank2

#include <stack>
#include <set>
#include <string>
#include <iostream>
#include <ctime>
#include <cstring>
#include <queue>
#include <algorithm>
/*#ifdef _BOTZONE_ONLINE
#include "jsoncpp/json.h"
#else
#include <jsoncpp/json.h>
#endif*/
#include "jsoncpp/json.h"

using std::string;
using std::cin;
using std::cout;
using std::endl;
using std::flush;
using std::getline;
using std::queue;
using std::vector;////
using std::priority_queue;////
namespace TankGame
{
	using std::stack;
	using std::set;
	using std::istream;

#ifdef _MSC_VER
#pragma region 常量定义和说明
#endif

	enum GameResult
	{
		NotFinished = -2,
		Draw = -1,
		Blue = 0,
		Red = 1
	};

	enum FieldItem
	{
		None = 0,
		Brick = 1,
		Steel = 2,
		Base = 4,
		Blue0 = 8,
		Blue1 = 16,
		Red0 = 32,
		Red1 = 64,
		Water = 128
	};

	template<typename T> inline T operator~ (T a) { return (T)~(int)a; }
	template<typename T> inline T operator| (T a, T b) { return (T)((int)a | (int)b); }
	template<typename T> inline T operator& (T a, T b) { return (T)((int)a & (int)b); }
	template<typename T> inline T operator^ (T a, T b) { return (T)((int)a ^ (int)b); }
	template<typename T> inline T& operator|= (T& a, T b) { return (T&)((int&)a |= (int)b); }
	template<typename T> inline T& operator&= (T& a, T b) { return (T&)((int&)a &= (int)b); }
	template<typename T> inline T& operator^= (T& a, T b) { return (T&)((int&)a ^= (int)b); }

	enum Action
	{
		Invalid = -2,
		Stay = -1,
		Up, Right, Down, Left,
		UpShoot, RightShoot, DownShoot, LeftShoot
	};

	// 坐标左上角为原点（0, 0），x 轴向右延伸，y 轴向下延伸
	// Side（对战双方） - 0 为蓝，1 为红
	// Tank（每方的坦克） - 0 为 0 号坦克，1 为 1 号坦克
	// Turn（回合编号） - 从 1 开始

	const int fieldHeight = 9, fieldWidth = 9, sideCount = 2, tankPerSide = 2;

	// 基地的横坐标
	const int baseX[sideCount] = { fieldWidth / 2, fieldWidth / 2 };

	// 基地的纵坐标
	const int baseY[sideCount] = { 0, fieldHeight - 1 };

	const int dx[4] = { 0, 1, 0, -1 }, dy[4] = { -1, 0, 1, 0 };
	const FieldItem tankItemTypes[sideCount][tankPerSide] = {
		{ Blue0, Blue1 },{ Red0, Red1 }
	};

	int maxTurn = 100;

#ifdef _MSC_VER
#pragma endregion

#pragma region 工具函数和类
#endif

	inline bool ActionIsMove(Action x)
	{
		return x >= Up && x <= Left;
	}

	inline bool ActionIsShoot(Action x)
	{
		return x >= UpShoot && x <= LeftShoot;
	}

	inline bool ActionDirectionIsOpposite(Action a, Action b)
	{
		return a >= Up && b >= Up && (a + 2) % 4 == b % 4;
	}

	inline bool CoordValid(int x, int y)
	{
		return x >= 0 && x < fieldWidth && y >= 0 && y < fieldHeight;
	}

	// 判断 item 是不是叠在一起的多个坦克
	inline bool HasMultipleTank(FieldItem item)
	{
		// 如果格子上只有一个物件，那么 item 的值是 2 的幂或 0
		// 对于数字 x，x & (x - 1) == 0 当且仅当 x 是 2 的幂或 0
		return !!(item & (item - 1));
	}

	inline int GetTankSide(FieldItem item)
	{
		return item == Blue0 || item == Blue1 ? Blue : Red;
	}

	inline int GetTankID(FieldItem item)
	{
		return item == Blue0 || item == Red0 ? 0 : 1;
	}

	// 获得动作的方向
	inline int ExtractDirectionFromAction(Action x)
	{
		if (x >= Up)
			return x % 4;
		return -1;
	}

	// 物件消失的记录，用于回退
	struct DisappearLog
	{
		FieldItem item;

		// 导致其消失的回合的编号
		int turn;

		int x, y;
		bool operator< (const DisappearLog& b) const
		{
			if (x == b.x)
			{
				if (y == b.y)
					return item < b.item;
				return y < b.y;
			}
			return x < b.x;
		}
	};

#ifdef _MSC_VER
#pragma endregion

#pragma region TankField 主要逻辑类
#endif

	class TankField
	{
	public:
		//!//!//!// 以下变量设计为只读，不推荐进行修改 //!//!//!//

		// 游戏场地上的物件（一个格子上可能有多个坦克）
		FieldItem gameField[fieldHeight][fieldWidth] = {};

		// 坦克是否存活
		bool tankAlive[sideCount][tankPerSide] = { { true, true },{ true, true } };

		// 基地是否存活
		bool baseAlive[sideCount] = { true, true };

		// 坦克横坐标，-1表示坦克已炸
		int tankX[sideCount][tankPerSide] = {
			{ fieldWidth / 2 - 2, fieldWidth / 2 + 2 },{ fieldWidth / 2 + 2, fieldWidth / 2 - 2 }
		};

		// 坦克纵坐标，-1表示坦克已炸
		int tankY[sideCount][tankPerSide] = { { 0, 0 },{ fieldHeight - 1, fieldHeight - 1 } };

		// 当前回合编号
		int currentTurn = 1;

		// 我是哪一方
		int mySide;

		// 用于回退的log
		stack<DisappearLog> logs;

		// 过往动作（previousActions[x] 表示所有人在第 x 回合的动作，第 0 回合的动作没有意义）
		Action previousActions[101][sideCount][tankPerSide] = { { { Stay, Stay },{ Stay, Stay } } };

		//!//!//!// 以上变量设计为只读，不推荐进行修改 //!//!//!//

		// 本回合双方即将执行的动作，需要手动填入
		Action nextAction[sideCount][tankPerSide] = { { Invalid, Invalid },{ Invalid, Invalid } };

		// 判断行为是否合法（出界或移动到非空格子算作非法）
		// 未考虑坦克是否存活
		bool ActionIsValid(int side, int tank, Action act)
		{
			if (act == Invalid)
				return false;
			if (act > Left && previousActions[currentTurn - 1][side][tank] > Left) // 连续两回合射击
				return false;
			if (act == Stay || act > Left)
				return true;
			int x = tankX[side][tank] + dx[act],
				y = tankY[side][tank] + dy[act];
			return CoordValid(x, y) && gameField[y][x] == None;// water cannot be stepped on//////处理水？？
		}

		// 判断 nextAction 中的所有行为是否都合法
		// 忽略掉未存活的坦克
		bool ActionIsValid()
		{
			for (int side = 0; side < sideCount; side++)
				for (int tank = 0; tank < tankPerSide; tank++)
					if (tankAlive[side][tank] && !ActionIsValid(side, tank, nextAction[side][tank]))
						return false;
			return true;
		}

		//added
		//判断本回合在(x,y)处的坦克能否射击
		bool CanShoot(int x, int y) {
			FieldItem tmp = gameField[y][x];
			return !(previousActions[currentTurn - 1][GetTankSide(tmp)][GetTankID(tmp)] > Left);
		}

		Action PreviousAction(int tanknum) {
			return previousActions[currentTurn - 1][mySide][tanknum];
		}

	private:
		void _destroyTank(int side, int tank)
		{
			tankAlive[side][tank] = false;
			tankX[side][tank] = tankY[side][tank] = -1;
		}

		void _revertTank(int side, int tank, DisappearLog& log)
		{
			int &currX = tankX[side][tank], &currY = tankY[side][tank];
			if (tankAlive[side][tank])
				gameField[currY][currX] &= ~tankItemTypes[side][tank];
			else
				tankAlive[side][tank] = true;
			currX = log.x;
			currY = log.y;
			gameField[currY][currX] |= tankItemTypes[side][tank];
		}
	public:

		// 执行 nextAction 中指定的行为并进入下一回合，返回行为是否合法
		bool DoAction()
		{
			if (!ActionIsValid())
				return false;

			// 1 移动
			for (int side = 0; side < sideCount; side++)
				for (int tank = 0; tank < tankPerSide; tank++)
				{
					Action act = nextAction[side][tank];

					// 保存动作
					previousActions[currentTurn][side][tank] = act;
					if (tankAlive[side][tank] && ActionIsMove(act))
					{
						int &x = tankX[side][tank], &y = tankY[side][tank];
						FieldItem &items = gameField[y][x];

						// 记录 Log
						DisappearLog log;
						log.x = x;
						log.y = y;
						log.item = tankItemTypes[side][tank];
						log.turn = currentTurn;
						logs.push(log);

						// 变更坐标
						x += dx[act];
						y += dy[act];

						// 更换标记（注意格子可能有多个坦克）
						gameField[y][x] |= log.item;
						items &= ~log.item;
					}
				}

			// 2 射♂击!
			set<DisappearLog> itemsToBeDestroyed;
			for (int side = 0; side < sideCount; side++)
				for (int tank = 0; tank < tankPerSide; tank++)
				{
					Action act = nextAction[side][tank];
					if (tankAlive[side][tank] && ActionIsShoot(act))
					{
						int dir = ExtractDirectionFromAction(act);
						int x = tankX[side][tank], y = tankY[side][tank];
						bool hasMultipleTankWithMe = HasMultipleTank(gameField[y][x]);
						while (true)
						{
							x += dx[dir];
							y += dy[dir];
							if (!CoordValid(x, y))
								break;
							FieldItem items = gameField[y][x];
							//tank will not be on water, and water will not be shot, so it can be handled as None
							if (items != None && items != Water)
							{
								// 对射判断
								if (items >= Blue0 &&
									!hasMultipleTankWithMe && !HasMultipleTank(items))
								{
									// 自己这里和射到的目标格子都只有一个坦克
									Action theirAction = nextAction[GetTankSide(items)][GetTankID(items)];
									if (ActionIsShoot(theirAction) &&
										ActionDirectionIsOpposite(act, theirAction))
									{
										// 而且我方和对方的射击方向是反的
										// 那么就忽视这次射击
										break;
									}
								}

								// 标记这些物件要被摧毁了（防止重复摧毁）
								for (int mask = 1; mask <= Red1; mask <<= 1)
									if (items & mask)
									{
										DisappearLog log;
										log.x = x;
										log.y = y;
										log.item = (FieldItem)mask;
										log.turn = currentTurn;
										itemsToBeDestroyed.insert(log);
									}
								break;
							}
						}
					}
				}

			for (auto& log : itemsToBeDestroyed)
			{
				switch (log.item)
				{
				case Base:
				{
					int side = log.x == baseX[Blue] && log.y == baseY[Blue] ? Blue : Red;
					baseAlive[side] = false;
					break;
				}
				case Blue0:
					_destroyTank(Blue, 0);
					break;
				case Blue1:
					_destroyTank(Blue, 1);
					break;
				case Red0:
					_destroyTank(Red, 0);
					break;
				case Red1:
					_destroyTank(Red, 1);
					break;
				case Steel:
					continue;
				default:
					;
				}
				gameField[log.y][log.x] &= ~log.item;
				logs.push(log);
			}

			for (int side = 0; side < sideCount; side++)
				for (int tank = 0; tank < tankPerSide; tank++)
					nextAction[side][tank] = Invalid;

			currentTurn++;
			return true;
		}

		// 回到上一回合
		bool Revert()
		{
			if (currentTurn == 1)
				return false;

			currentTurn--;
			while (!logs.empty())
			{
				DisappearLog& log = logs.top();
				if (log.turn == currentTurn)
				{
					logs.pop();
					switch (log.item)
					{
					case Base:
					{
						int side = log.x == baseX[Blue] && log.y == baseY[Blue] ? Blue : Red;
						baseAlive[side] = true;
						gameField[log.y][log.x] = Base;
						break;
					}
					case Brick:
						gameField[log.y][log.x] = Brick;
						break;
					case Blue0:
						_revertTank(Blue, 0, log);
						break;
					case Blue1:
						_revertTank(Blue, 1, log);
						break;
					case Red0:
						_revertTank(Red, 0, log);
						break;
					case Red1:
						_revertTank(Red, 1, log);
						break;
					default:
						;
					}
				}
				else
					break;
			}
			return true;
		}

		// 游戏是否结束？谁赢了？
		GameResult GetGameResult()
		{
			bool fail[sideCount] = {};
			for (int side = 0; side < sideCount; side++)
				if ((!tankAlive[side][0] && !tankAlive[side][1]) || !baseAlive[side])
					fail[side] = true;
			if (fail[0] == fail[1])
				return fail[0] || currentTurn > maxTurn ? Draw : NotFinished;
			if (fail[Blue])
				return Red;
			return Blue;
		}

		/* 三个 int 表示场地 01 矩阵（每个 int 用 27 位表示 3 行）
		   initialize gameField[][]
		   brick>water>steel
		*/
		TankField(int hasBrick[3], int hasWater[3], int hasSteel[3], int mySide) : mySide(mySide)
		{
			for (int i = 0; i < 3; i++)
			{
				int mask = 1;
				for (int y = i * 3; y < (i + 1) * 3; y++)
				{
					for (int x = 0; x < fieldWidth; x++)
					{
						if (hasBrick[i] & mask)
							gameField[y][x] = Brick;
						else if (hasWater[i] & mask)
							gameField[y][x] = Water;
						else if (hasSteel[i] & mask)
							gameField[y][x] = Steel;
						mask <<= 1;
					}
				}
			}
			for (int side = 0; side < sideCount; side++)
			{
				for (int tank = 0; tank < tankPerSide; tank++)
					gameField[tankY[side][tank]][tankX[side][tank]] = tankItemTypes[side][tank];
				gameField[baseY[side]][baseX[side]] = Base;
			}
		}
		// 打印场地
		void DebugPrint()
		{
#ifndef _BOTZONE_ONLINE
			const string side2String[] = { "蓝", "红" };
			const string boolean2String[] = { "已炸", "存活" };
			const char* boldHR = "==============================";
			const char* slimHR = "------------------------------";
			cout << boldHR << endl
				<< "图例：" << endl
				<< ". - 空\t# - 砖\t% - 钢\t* - 基地\t@ - 多个坦克" << endl
				<< "b - 蓝0\tB - 蓝1\tr - 红0\tR - 红1\tW - 水" << endl //Tank2 feature
				<< slimHR << endl;
			for (int y = 0; y < fieldHeight; y++)
			{
				for (int x = 0; x < fieldWidth; x++)
				{
					switch (gameField[y][x])
					{
					case None:
						cout << '.';
						break;
					case Brick:
						cout << '#';
						break;
					case Steel:
						cout << '%';
						break;
					case Base:
						cout << '*';
						break;
					case Blue0:
						cout << 'b';
						break;
					case Blue1:
						cout << 'B';
						break;
					case Red0:
						cout << 'r';
						break;
					case Red1:
						cout << 'R';
						break;
					case Water:
						cout << 'W';
						break;
					default:
						cout << '@';
						break;
					}
				}
				cout << endl;
			}
			cout << slimHR << endl;
			for (int side = 0; side < sideCount; side++)
			{
				cout << side2String[side] << "：基地" << boolean2String[baseAlive[side]];
				for (int tank = 0; tank < tankPerSide; tank++)
					cout << ", 坦克" << tank << boolean2String[tankAlive[side][tank]];
				cout << endl;
			}
			cout << "当前回合：" << currentTurn << "，";
			GameResult result = GetGameResult();
			if (result == -2)
				cout << "游戏尚未结束" << endl;
			else if (result == -1)
				cout << "游戏平局" << endl;
			else
				cout << side2String[result] << "方胜利" << endl;
			cout << boldHR << endl;
#endif
		}

		bool operator!= (const TankField& b) const
		{

			for (int y = 0; y < fieldHeight; y++)
				for (int x = 0; x < fieldWidth; x++)
					if (gameField[y][x] != b.gameField[y][x])
						return true;

			for (int side = 0; side < sideCount; side++)
				for (int tank = 0; tank < tankPerSide; tank++)
				{
					if (tankX[side][tank] != b.tankX[side][tank])
						return true;
					if (tankY[side][tank] != b.tankY[side][tank])
						return true;
					if (tankAlive[side][tank] != b.tankAlive[side][tank])
						return true;
				}

			if (baseAlive[0] != b.baseAlive[0] ||
				baseAlive[1] != b.baseAlive[1])
				return true;

			if (currentTurn != b.currentTurn)
				return true;

			return false;
		}
	};

#ifdef _MSC_VER
#pragma endregion
#endif

	TankField *field;

#ifdef _MSC_VER
#pragma region 与平台交互部分
#endif

	// 内部函数
	namespace Internals
	{
		Json::Reader reader;
#ifdef _BOTZONE_ONLINE
		Json::FastWriter writer;
#else
		Json::StyledWriter writer;
#endif

		void _processRequestOrResponse(Json::Value& value, bool isOpponent)
		{
			if (value.isArray())
			{
				if (!isOpponent)
				{
					for (int tank = 0; tank < tankPerSide; tank++)
						field->nextAction[field->mySide][tank] = (Action)value[tank].asInt();
				}
				else
				{
					for (int tank = 0; tank < tankPerSide; tank++)
						field->nextAction[1 - field->mySide][tank] = (Action)value[tank].asInt();
					field->DoAction();
				}
			}
			else
			{
				// 是第一回合，裁判在介绍场地
				int hasBrick[3], hasWater[3], hasSteel[3];
				for (int i = 0; i < 3; i++) {//Tank2 feature(???????????????)
					hasWater[i] = value["waterfield"][i].asInt();
					hasBrick[i] = value["brickfield"][i].asInt();
					hasSteel[i] = value["steelfield"][i].asInt();
				}
				field = new TankField(hasBrick, hasWater, hasSteel, value["mySide"].asInt());
			}
		}

		// 请使用 SubmitAndExit 或者 SubmitAndDontExit
		void _submitAction(Action tank0, Action tank1, string debug = "", string data = "", string globaldata = "")
		{
			Json::Value output(Json::objectValue), response(Json::arrayValue);
			response[0U] = tank0;
			response[1U] = tank1;
			output["response"] = response;
			if (!debug.empty())
				output["debug"] = debug;
			if (!data.empty())
				output["data"] = data;
			if (!globaldata.empty())
				output["globaldata"] = globaldata;
			cout << writer.write(output) << endl;
		}
	}

	// 从输入流（例如 cin 或者 fstream）读取回合信息，存入 TankField，并提取上回合存储的 data 和 globaldata
	// 本地调试的时候支持多行，但是最后一行需要以没有缩进的一个"}"或"]"结尾
	void ReadInput(istream& in, string& outData, string& outGlobalData)
	{
		Json::Value input;
		string inputString;
		do
		{
			getline(in, inputString);
		} while (inputString.empty());
#ifndef _BOTZONE_ONLINE
		// 猜测是单行还是多行
		char lastChar = inputString[inputString.size() - 1];
		if (lastChar != '}' && lastChar != ']')
		{
			// 第一行不以}或]结尾，猜测是多行
			string newString;
			do
			{
				getline(in, newString);
				inputString += newString;
			} while (newString != "}" && newString != "]");
		}
#endif
		Internals::reader.parse(inputString, input);

		if (input.isObject())
		{
			Json::Value requests = input["requests"], responses = input["responses"];
			if (!requests.isNull() && requests.isArray())
			{
				size_t i, n = requests.size();
				for (i = 0; i < n; i++)
				{
					Internals::_processRequestOrResponse(requests[i], true);
					if (i < n - 1)
						Internals::_processRequestOrResponse(responses[i], false);
				}
				outData = input["data"].asString();
				outGlobalData = input["globaldata"].asString();
				return;
			}
		}
		Internals::_processRequestOrResponse(input, true);
	}

	// 提交决策并退出，下回合时会重新运行程序
	void SubmitAndExit(Action tank0, Action tank1, string debug = "", string data = "", string globaldata = "")
	{
		Internals::_submitAction(tank0, tank1, debug, data, globaldata);
#ifdef _BOTZONE_ONLINE
		exit(0);
#endif
	}

	// 提交决策，下回合时程序继续运行（需要在 Botzone 上提交 Bot 时选择“允许长时运行”）
	// 如果游戏结束，程序会被系统杀死
	void SubmitAndDontExit(Action tank0, Action tank1)
	{
		Internals::_submitAction(tank0, tank1);
		field->nextAction[field->mySide][0] = tank0;
		field->nextAction[field->mySide][1] = tank1;
		cout << ">>>BOTZONE_REQUEST_KEEP_RUNNING<<<" << endl;
	}
#ifdef _MSC_VER
#pragma endregion
#endif
}

using namespace TankGame;
//计算在同一条直线上的坦克之间的障碍物数量
int obstacle(int x1, int y1, int x2, int y2) {
	int cnt = 0;
	if (x1 == x2) {//横坐标相等
		if (y1 < y2) {
			for (int i = y1 + 1; i < y2; ++i) {
				if (field->gameField[i][x1] == Steel)
					return 0xfffff;//若路径上有铁墙相隔，则视作有无穷堵砖墙，返回一个极大的数
				else if (field->gameField[i][x1] == Brick)
					cnt++;//路径上每出现一块贴墙，障碍数加一
			}
		}
		else {
			for (int i = y2 + 1; i < y1; ++i) {
				if (field->gameField[i][x1] == Steel)
					return 0xfffff;//若路径上有铁墙相隔，则视作有无穷堵砖墙，返回一个极大的数
				else if (field->gameField[i][x1] == Brick)
					cnt++;//路径上每出现一块贴墙，障碍数加一
			}
		}
	}
	else {//纵坐标相等
		if (x1 < x2) {
			for (int i = x1 + 1; i < x2; ++i) {
				if (field->gameField[y1][i] == Steel)
					return 0xfffff;//若路径上有铁墙相隔，则视作有无穷堵砖墙，返回一个极大的数
				else if (field->gameField[y1][i] == Brick)
					cnt++;//路径上每出现一块贴墙，障碍数加一
			}
		}
		else {
			for (int i = x2 + 1; i < x1; ++i) {
				if (field->gameField[y1][i] == Steel)
					return 0xfffff;//若路径上有铁墙相隔，则视作有无穷堵砖墙，返回一个极大的数
				else if (field->gameField[y1][i] == Brick)
					cnt++;//路径上每出现一块贴墙，障碍数加一
			}
		}
	}
	return cnt;
}

//判断当先坦克的最短路径决策是否会使自己处于危险的境地
int danger(int myx, int myy, int sbx, int sby, int resx, int resy) {
	int dir = (field->mySide == 1 ? -1 : 1);//我方坦克的移动方向
	if (abs(myx - sbx) >= 2 && abs(myy - sby) >= 2) return false;
	//如果我方坦克与敌方坦克的横纵距离均大于等于2，则无论下一回合自己和对手作出怎样的决策，自己要么有子弹，要么和对手不再同一条直线上，自己下一回合是不会死的
	if (abs(myx - sbx) == 1 && abs(myy - sby) == 1) {
		if ((field->gameField[myy][sbx] == Steel || field->gameField[myy][sbx] == Water) &&
			(field->gameField[sby][myx] == Steel || field->gameField[sby][myx] == Water)) {
			return false;
		}
		return true;
	}//如果两架坦克处于对角线上的相邻位置
	if (abs(myx - sbx) == 1 && abs(myy - sby) >= 2) {
		if (resx == sbx && resy == myy && field->gameField[sby][myx] == None && obstacle(resx, resy, sbx, sby) == 0 &&
			field->CanShoot(sbx, sby))
			return true;//如果我下一步将会移动到和敌军坦克在一条直线上，并且敌军坦克和我的坦克之间没有障碍，并且敌人是可以射击的
		if (resx == sbx && resy == myy && field->gameField[myy][sbx] == None && obstacle(resx, resy, sbx, sby) == 0 &&
			field->CanShoot(sbx, sby))
			return true;
		if (resx == myx && resy == myy + dir && field->gameField[resy][resx] == Brick && field->gameField[sby][resx] == None && obstacle(resx, myy, resx, sby) == 1)
			return true;//如果我下一步轰掉了砖墙之后可能会导致敌军坦克和我军坦克在同一条直线上，并且敌军坦克是可以射击的
		//概率不高，暂时不做考虑
		//if (resx == myx && resy == myy + dir && field->gameField[resy][resx] == Brick && field->gameField[myy][sbx] == None && obstacle(resx, myy, sbx, myy) == 0)
		//	return true;
		return false;
	}
	if (abs(myy - sby) == 1 && abs(myx - sbx) >= 2) {
		if (resy == sby && resx == myx && field->gameField[sby][myx] == None && obstacle(resx, resy, sbx, sby) == 0 &&
			field->CanShoot(sbx, sby))
			return 2;

		int dir2 = (myx < sbx ? 1 : -1);
		if (resy == myy && resx == myx + dir2 && field->gameField[resy][resx] == Brick && field->gameField[resy][sbx] == None && obstacle(myx, resy, sbx, resy) == 1) {
			return 2;
		}
		//概率不高，暂时不做考虑

		return false;
	}
	//共线情形
	if (myx == sbx) {
		if (obstacle(myx, myy, sbx, sby) == 1 && resy == myy + dir && resx == myx && field->gameField[resy][resx] == Brick)
			return true;
		return false;
	}//如果坦克之间的障碍物只有一堵墙，并且我方坦克下一步的决策是轰炸那堵墙，那就很危险
	if (myy == sby) {
		int dir2 = (myx < sbx ? 1 : -1);
		if (obstacle(myx, myy, sbx, sby) == 1 && resx == myx + dir2 && resy == myy && field->gameField[resy][resx] == Brick)
			return 2;
		return false;
	}
	return false;
}

int steps[9][9] = { 0 };
int accessed[9][9] = { 0 };
struct xy {
	int x, y;
	bool operator<(const xy &a) const {////
		if (steps[y][x] > steps[a.y][a.x])
			return true;
		else if (steps[y][x] == steps[a.y][a.x]) {
			if ((y == baseY[1 - field->mySide] || x == baseX[1 - field->mySide]) && !(a.y == baseY[1 - field->mySide] || a.x == baseX[1 - field->mySide])) return true;
		}
		return false;
	}
}node, top;

bool compare(const xy& x, const xy& y) {
	if (abs(x.x - baseX[1 - field->mySide]) < abs(y.x - baseX[1 - field->mySide])) return true;
	return false;
}
/*bool operator==(const xy &a, const xy &b) {/////
	return steps[a.x][a.y] == steps[b.x][b.y];
}*/

//using namespace std;
vector <xy> nextstep;
void findpath(xy curpos, xy findpos) {
	int ex = baseX[1 - field->mySide];
	int ey = baseY[1 - field->mySide];
	xy newpos;
	for (int i = 0; i < 4; ++i) {
		newpos.x = curpos.x + dx[i];
		newpos.y = curpos.y + dy[i];
		/*if (newpos.x == findpos.x&&newpos.y == findpos.y) {
			nextstep.push_back(curpos);
			return;
		}*/
		if (!CoordValid(newpos.x, newpos.y) || field->gameField[newpos.y][newpos.x] == Steel || field->gameField[newpos.y][newpos.x] == Water) continue;
		else if (field->gameField[curpos.y][curpos.x] == Brick) {
			/*if (newpos.x == baseX[1 - field->mySide] || newpos.y == baseY[1 - field->mySide]) {
				if (steps[newpos.y][newpos.x] == steps[curpos.y][curpos.x] - 1) {
					findpath(newpos, findpos);
				}
			}*/
			if ((newpos.y == ey && curpos.y == ey)) {
				if (steps[newpos.y][newpos.x] == steps[curpos.y][curpos.x] - 1) {
					if (newpos.x == findpos.x&&newpos.y == findpos.y) {
						nextstep.push_back(curpos);
						return;
					}
					findpath(newpos, findpos);
				}
			}
			else if (newpos.x == ex && curpos.x == ex) {
				if ((field->mySide == Blue && newpos.y > 4) || (field->mySide == Red && newpos.y < 4)) {
					if (steps[newpos.y][newpos.x] == steps[curpos.y][curpos.x] - 1) {
						if (newpos.x == findpos.x&&newpos.y == findpos.y) {
							nextstep.push_back(curpos);
							return;
						}
						findpath(newpos, findpos);
					}
				}
			}
			else {
				if (steps[newpos.y][newpos.x] == steps[curpos.y][curpos.x] - 2) {
					if (newpos.x == findpos.x&&newpos.y == findpos.y) {
						nextstep.push_back(curpos);
						return;
					}
					findpath(newpos, findpos);
				}
			}
		}
		else {
			/*if (newpos.x == baseX[1 - field->mySide] || newpos.y == baseY[1 - field->mySide]) {
				if (steps[newpos.y][newpos.x] == steps[curpos.y][curpos.x]) {
					if (steps[newpos.y][newpos.x] == steps[findpos.y][findpos.x]) {
						nextstep.push_back(curpos); return;
					}
					else findpath(newpos, findpos);
				}
			}*/
			if ((newpos.y == ey && curpos.y == ey)) {
				if (steps[newpos.y][newpos.x] == steps[curpos.y][curpos.x]) {
					if (newpos.x == findpos.x&&newpos.y == findpos.y) {
						nextstep.push_back(curpos);
						return;
					}
					if (abs(newpos.x - baseX[1 - field->mySide]) > abs(curpos.x - baseX[1 - field->mySide]) || abs(newpos.y - baseY[1 - field->mySide]) > abs(curpos.y - baseY[1 - field->mySide])) findpath(newpos, findpos);
				}
			}
			else if (newpos.x == ex && curpos.x == ex) {
				if ((field->mySide == Blue && newpos.y > 4) || (field->mySide == Red && newpos.y < 4)) {
					if (steps[newpos.y][newpos.x] == steps[curpos.y][curpos.x]) {
						if (newpos.x == findpos.x&&newpos.y == findpos.y) {
							nextstep.push_back(curpos);
							return;
						}
						if (abs(newpos.x - baseX[1 - field->mySide]) > abs(curpos.x - baseX[1 - field->mySide]) || abs(newpos.y - baseY[1 - field->mySide]) > abs(curpos.y - baseY[1 - field->mySide])) findpath(newpos, findpos);
					}
				}
			}
			else {
				if (steps[newpos.y][newpos.x] == steps[curpos.y][curpos.x] - 1) {
					if (newpos.x == findpos.x&&newpos.y == findpos.y) {
						nextstep.push_back(curpos);
						return;
					}
					findpath(newpos, findpos);
				}
			}
		}
	}
	return;
}

int RandBetween(int from, int to);

TankGame::Action RandAction(int tank);

Action Decision(int tanknum) {
	Action act = Stay;
	int myx = field->tankX[field->mySide][tanknum];
	int myy = field->tankY[field->mySide][tanknum];
	if (myx == -1) {
		return act;
	}
	//////int dir = field->mySide ? 1 : -1;
	int ex = baseX[1 - field->mySide];
	int ey = baseY[1 - field->mySide];
	int sbx[2] = { field->tankX[1 - field->mySide][0],field->tankX[1 - field->mySide][1] };
	int sby[2] = { field->tankY[1 - field->mySide][0],field->tankY[1 - field->mySide][1] };
	//共线
	/*
	bool flag = true;
	for(int i=0;i<2;++i){
		bool flag=true;
		if(myx==sbx[i]&&myy==sby[i]){
			act=(dir==-1?Up:Down);
			return act;//可能被墙或水挡住
		}

		if(myx==sbx[i]&&obstacle(myx,myy,sbx[i],sby[i])==0&&field->CanShoot(myx,myy)){
			act = (myy > sby[i] ? UpShoot : DownShoot);
			return act;
		}
		else if(myy==sby[i]&&obstacle(myx,myy,sbx[i],sby[i])==0&&field->CanShoot(myx,myy)){
			act = (myx > sbx[i] ? LeftShoot : RightShoot);
			return act;
		}
	}
	*/
	//bfs
	for (int i = 0; i < 9; ++i)
		for (int j = 0; j < 9; ++j)
			steps[i][j] = -1;
	for (int i = 0; i < 9; ++i)
		for (int j = 0; j < 9; ++j)
			accessed[i][j] = 0;
	accessed[myy][myx] = 1;
	steps[myy][myx] = 0;
	priority_queue<xy> Q;
	node.x = myx;
	node.y = myy;
	Q.push(node);
	int newX = 0, newY = 0;///
	while (!Q.empty()) {
		int flag = 0;
		top = Q.top();
		Q.pop();
		for (int j = 0; j < 4; ++j) {
			newX = top.x + dx[j];///
			newY = top.y + dy[j];///
			if (!CoordValid(newX, newY) ||
				field->gameField[newY][newX] == Steel || field->gameField[newY][newX] == Water)
				continue;
			if (!accessed[newY][newX]) {
				node.x = newX;
				node.y = newY;
				accessed[newY][newX] = 1;
				if (field->gameField[newY][newX] == Brick)
					steps[newY][newX] = steps[top.y][top.x] + 2;
				else steps[newY][newX] = steps[top.y][top.x] + 1;
				if ((newY == ey && top.y == ey)) --steps[newY][newX];
				else if (newX == ex && top.x == ex) {
					if ((field->mySide == Blue && newY > 4) || (field->mySide == Red && newY < 4)) --steps[newY][newX];
				}
				if (newX == baseX[1 - field->mySide] && newY == baseY[1 - field->mySide]) {
					flag = 1;
					break;
				}
				Q.push(node);
			}
		}
		if (flag)break;
	}
	node.x = newX; //bfs之后，newX，newY应该是基地坐标了
	node.y = newY;
	xy cur;
	cur.x = myx;
	cur.y = myy;
	nextstep.clear();
	findpath(node, cur);
	sort(nextstep.begin(), nextstep.end(), compare);
	unique(nextstep.begin(), nextstep.end(), compare);
	/*
	int tx = ex, ty = ey;
	int _tx = ex, _ty = ey;
	while (tx != myx && ty != myy) {
		for (int j = 0; j < 4; ++j) {
			int newX = tx + dx[j], newY = ty + dy[j];
			if (!CoordValid(newX, newY) ||
				tankItemTypes[newX][newY] == Steel || tankItemTypes[newX][newY] == Water)
				continue;
			if (!accessed[newX][newY])
				continue;
			if (tankItemTypes[newX][newY] == Brick) {
				if (((newX == ex && tx == ex) || (newY == ey && ty == ey)) &&
					steps[newX][newY] == steps[tx][ty] - 1) {
					_tx = tx;
					_ty = ty;
					tx = newX;
					ty = newY;
					break;
				} else if (steps[newX][newY] == steps[tx][ty] - 2) {
					_tx = tx;
					_ty = ty;
					tx = newX;
					ty = newY;
					break;
				}
			} else {
				if (((newX == ex && tx == ex) || (newY == ey && ty == ey)) && steps[newX][newY] == steps[tx][ty]) {
					_tx = tx;
					_ty = ty;
					tx = newX;
					ty = newY;
					break;
				} else if (steps[newX][newY] == steps[tx][ty] - 1) {
					_tx = tx;
					_ty = ty;
					tx = newX;
					ty = newY;
					break;
				}
			}
		}
	}
	*/
	//共线情况
	bool flag = true;
	bool flag2 = false;
	//int _tx = 0, _ty = 0, tx = ex, ty = ey;////
	for (int i = 0; i < 2; ++i) {
		if (myx == sbx[i] && myy == sby[i]) {
			flag2 = true;
			/*for (size_t j = 0; j < nextstep.size(); ++j) {////
				_tx = nextstep[j].x;////
				_ty = nextstep[j].y;////
				if (field->gameField[_ty][_tx] == Brick) {
					if (_tx - tx == 0 && _ty - ty == -1)act = UpShoot;
					else if (_tx - tx == 1 && _ty - ty == 0)act = RightShoot;
					else if (_tx - tx == 0 && _ty - ty == 1)act = DownShoot;
					else act = LeftShoot;
				}
				else {
					if (_tx - tx == 0 && _ty - ty == -1)act = Up;
					else if (_tx - tx == 1 && _ty - ty == 0)act = Right;
					else if (_tx - tx == 0 && _ty - ty == 1)act = Down;
					else act = Left;
				}
			}*/
		}
		else if (myx == sbx[i]) {
			if (obstacle(myx, myy, sbx[i], sby[i])) continue;
			flag2 = true;
			if (field->CanShoot(myx, myy)) {
				for (size_t j = 0; j < nextstep.size(); ++j) {/////
					int _tx = nextstep[j].x;
					int _ty = nextstep[j].y;
					if (_tx != myx) {
						if (field->gameField[_ty][_tx] == Brick) continue;
						else if (_ty == baseY[1 - field->mySide] && _tx == baseX[1 - field->mySide]) {
							act = (_tx > myx ? RightShoot : LeftShoot);
							flag = false;
							break;
						}
						else {
							act = (_tx > myx ? Right : Left);
							flag = false;
							break;
						}
					}
				}
				if (flag) act = (myy > sby[i] ? UpShoot : DownShoot);////
			}
			else {
				//不再是往前，而是根据bfs做出决定
				for (size_t j = 0; j < nextstep.size(); ++j) {//////
					int _tx = nextstep[j].x;
					int _ty = nextstep[j].y;
					int tx = myx;
					int ty = myy;
					if (field->gameField[_ty][_tx] == Brick) continue;
					if (_ty == sby[i] && _tx == sbx[i]) continue;
					else {
						if (_tx - tx == 0 && _ty - ty == -1)act = Up;
						else if (_tx - tx == 1 && _ty - ty == 0)act = Right;
						else if (_tx - tx == 0 && _ty - ty == 1)act = Down;
						else act = Left;
						flag = false;
						break;
					}
				}
			}
		}
		else if (myy == sby[i]) {
			if (obstacle(myx, myy, sbx[i], sby[i])) continue;
			flag2 = true;
			if (field->CanShoot(myx, myy)) {
				for (size_t j = 0; j < nextstep.size(); ++j) {///////
					int _tx = nextstep[j].x;
					int _ty = nextstep[j].y;
					if (_ty != myy) {
						if (field->gameField[_ty][_tx] == Brick) continue;
						else if (_ty == baseY[1 - field->mySide] && _tx == baseX[1 - field->mySide]) {
							act = (_ty > myy ? DownShoot : UpShoot);
							flag = false;
							break;
						}
						else {
							act = (_ty > myy ? Down : Up);
							flag = false;
							break;
						}
					}
				}
				if (flag) act = (myx > sbx[i] ? LeftShoot : RightShoot);////
			}
			else {
				//不再是往前，而是根据bfs做出决定
				for (size_t j = 0; j < nextstep.size(); ++j) {/////
					int _tx = nextstep[j].x;
					int _ty = nextstep[j].y;
					int tx = myx;
					int ty = myy;
					if (_ty == sby[i] && _tx == sbx[i]) continue;
					if (field->gameField[_ty][_tx] == Brick) continue;
					else {
						if (_tx - tx == 0 && _ty - ty == -1)act = Up;
						else if (_tx - tx == 1 && _ty - ty == 0)act = Right;
						else if (_tx - tx == 0 && _ty - ty == 1)act = Down;
						else act = Left;
						flag = false;
						break;
					}
				}
			}
		}
		if (flag2) return act;
	}
	if (myy == baseY[1 - field->mySide]) {
		if (field->CanShoot(myx, myy)) {
			act = (myx > baseX[1 - field->mySide] ? LeftShoot : RightShoot);
			return act;
		}
	}
	if (myx == baseX[1 - field->mySide]) {
		if (field->CanShoot(myx, myy)) {
			act = (myy > baseY[1 - field->mySide] ? UpShoot : DownShoot);
			return act;
		}
	}

	int _ty = nextstep[0].y;
	int _tx = nextstep[0].x;
	int tx = myx;
	int ty = myy;
	if (field->gameField[_ty][_tx] == Brick) {
		if (_tx - tx == 0 && _ty - ty == -1)act = UpShoot;
		else if (_tx - tx == 1 && _ty - ty == 0)act = RightShoot;
		else if (_tx - tx == 0 && _ty - ty == 1)act = DownShoot;
		else act = LeftShoot;
	}
	else {
		if (_tx - tx == 0 && _ty - ty == -1)act = Up;
		else if (_tx - tx == 1 && _ty - ty == 0)act = Right;
		else if (_tx - tx == 0 && _ty - ty == 1)act = Down;
		else act = Left;
	}
	bool flag3 = false;
	//invalid
	if (ActionIsShoot(act) && !field->CanShoot(myx, myy))
		act = Stay;

	//dangers,将可能造成danger的那一步设成钢墙，表示不能走这里
	if (danger(myx, myy, sbx[0], sby[0], _tx, _ty) == 1 || danger(myx, myy, sbx[1], sby[1], _tx, _ty) == 1) {
		flag3 = true;
		act = Stay;
	}
	else if (danger(myx, myy, sbx[0], sby[0], _tx, _ty) == 2 || danger(myx, myy, sbx[1], sby[1], _tx, _ty) == 2) {
		flag3 = true;
		FieldItem currentgrid = field->gameField[_ty][_tx];
		field->gameField[_ty][_tx] = Steel;
		int oristep = steps[baseY[1 - field->mySide]][baseX[1 - field->mySide]];
		for (int i = 0; i < 9; ++i)
			for (int j = 0; j < 9; ++j)
				steps[i][j] = -1;
		for (int i = 0; i < 9; ++i)
			for (int j = 0; j < 9; ++j)
				accessed[i][j] = 0;
		accessed[myy][myx] = 1;
		steps[myy][myx] = 0;
		//priority_queue<xy> Q;
		while (!Q.empty()) Q.pop();
		node.x = myx;
		node.y = myy;
		Q.push(node);
		int newX = 0, newY = 0;///
		while (!Q.empty()) {
			int flag = 0;
			top = Q.top();
			Q.pop();
			for (int j = 0; j < 4; ++j) {
				newX = top.x + dx[j];///
				newY = top.y + dy[j];///
				if (!CoordValid(newX, newY) ||
					field->gameField[newY][newX] == Steel || field->gameField[newY][newX] == Water)
					continue;
				if (!accessed[newY][newX]) {
					node.x = newX;
					node.y = newY;
					accessed[newY][newX] = 1;
					if (field->gameField[newY][newX] == Brick)
						steps[newY][newX] = steps[top.y][top.x] + 2;
					else steps[newY][newX] = steps[top.y][top.x] + 1;
					if ((newY == ey && top.y == ey)) --steps[newY][newX];
					else if (newX == ex && top.x == ex) {
						if ((field->mySide == Blue && newY > 4) || (field->mySide == Red && newY < 4)) --steps[newY][newX];
					}
					if (newX == baseX[1 - field->mySide] && newY == baseY[1 - field->mySide]) {
						flag = 1;
						break;
					}
					Q.push(node);
				}
			}
			if (flag)break;
		}
		node.x = newX; //bfs之后，newX，newY应该是基地坐标了
		node.y = newY;
		xy cur;
		cur.x = myx;
		cur.y = myy;
		nextstep.clear();
		findpath(node, cur);
		sort(nextstep.begin(), nextstep.end(), compare);
		_ty = nextstep[0].y;
		_tx = nextstep[0].x;
		tx = myx;
		ty = myy;
		if (steps[baseY[1 - field->mySide]][baseX[1 - field->mySide]] > oristep + 3) {
			act = Stay;
			_tx = tx;
			_ty = ty;
		}
		if (field->gameField[_ty][_tx] == Brick) {
			if (_tx - tx == 0 && _ty - ty == -1)act = UpShoot;
			else if (_tx - tx == 1 && _ty - ty == 0)act = RightShoot;
			else if (_tx - tx == 0 && _ty - ty == 1)act = DownShoot;
			else if (_tx - tx == -1 && _ty - ty == 0) act = LeftShoot;
			//else act = Stay;
		}
		else {
			if (_tx - tx == 0 && _ty - ty == -1)act = Up;
			else if (_tx - tx == 1 && _ty - ty == 0)act = Right;
			else if (_tx - tx == 0 && _ty - ty == 1)act = Down;
			else if (_tx - tx == -1 && _ty - ty == 0) act = Left;
			//else act = Stay;
		}

		//invalid
		if (ActionIsShoot(act) && !field->CanShoot(myx, myy))
			act = Stay;
		if (danger(myx, myy, sbx[0], sby[0], _tx, _ty) || danger(myx, myy, sbx[1], sby[1], _tx, _ty))
			act = Stay;
		field->gameField[_tx][_ty] = currentgrid;
	}
	if (abs(myx - baseX[1 - field->mySide] <= 1) && abs(myy - baseY[1 - field->mySide]) <= 2) {
		if (ActionIsShoot(field->PreviousAction(tanknum))) {
			act = Action(field->PreviousAction(tanknum) - 4);
		}
	}

	//field->gameField[_ty][_tx] = Steel;
	//act = Stay;
	if (TankGame::field->ActionIsValid(TankGame::field->mySide, tanknum, act))/////
		return act;
	else///////
		return RandAction(tanknum);///////
}

int RandBetween(int from, int to)
{
	return rand() % (to - from) + from;
}

TankGame::Action RandAction(int tank)
{
	while (true)
	{
		int temp = RandBetween(TankGame::Stay, TankGame::LeftShoot + 1);
		auto act = (TankGame::Action)temp;
		if (TankGame::field->ActionIsValid(TankGame::field->mySide, tank, act))
			return act;
	}
}

int main()
{
	//srand((unsigned)time(nullptr));
	string data, globaldata;
	while (1) {

		TankGame::ReadInput(cin, data, globaldata);
		field->DebugPrint();
		TankGame::SubmitAndExit(Decision(0), Decision(1));////
		//field->DebugPrint();
	}
	//system("pause");
}