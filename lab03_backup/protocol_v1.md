客户--客户
	包类型：需要的信息（挑战者：A；被挑战者：B）
	1. 挑战：A的ID、B的ID
		1.1.1 A-S：“您向B发起了挑战”					--A
			[opcode=OP_CHALLENGE_RQT][flags=0]
			[opr_cnt=2]
			[id = challengerID][id = recipientID]
			“A向B发起了挑战”							--S
		1.1.2 S-A：【ACK】“您对B的挑战等待B的回应”		--A
			“A对B的挑战请求已经发送给B”					--S
		1.2.1 S-B：“A向您发起了挑战，是否接收？”		--B

	2. 挑战结果：A的ID、B的ID
		2.1.1 B-S：【ACK-拒绝】“您拒绝了A的挑战”		--B
			[opcode=OP_CHALLENGE_RPL][flags=FLAG_REJECT]
			[opr_cnt=2]
			[id = challengerID][id = recipientID]
			“B拒绝了A的挑战”							--S
		2.1.2 S-A：“B拒绝了您的挑战”					--A

		2.2.1 B-S：【ACK-接受】“您接收了A的挑战，战斗开始”	--B
			[opcode=OP_CHALLENGE_RPL][flags=FLAG_ACCEPT]
			[opr_cnt=2]
			[id = challengerID][id = recipientID]
			“B接收了A的挑战，开始战斗”					--S
		2.2.2 S-A：“B接受了您的挑战，战斗开始”			--A

	4. 战斗：A的ID、A输入的字符串
		4.1.1 A-S：“您输入XXX，等待B输入”				--A
			[opcode=OP_BATTLE_INPUT][flags=0][opr_cnt=2]
			[userid][input content]
		4.1.2 B-S：“您输入XXX，等待A输入”				--B
			[opcode][A][B][B-series]
		4.2.1 S-A/S-B：“A输入XXX，B输入XXX，战斗开始”	--S
			[opcode=OP_BATTLE_MSG][flags=FLAG_BATTLE_INTRO]
			[opr_cnt=4]
			[userid][input content][userid][input content]
		4.2.2 S-A/S-B：“A出XX，B出XX，结果XX”
			[opcode=OP_BATTLE_MSG][flags=FLAG_BATTLE_EACH]
			[opr_cnt=5]
			[userid][move_name][userid][move_name][winner-id]
		...重复数次
	6. 战斗结束：A的ID&结果、B的ID&结果
		6.1：S-A/S-B：“战斗结束，您赢/输了”
			[opcode=OP_BATTLE_RESULT][flags=0]
			[opr_cnt=8]
			[user_id][win][lose][draw]
			[user_id][win][lose][draw]

	8. 弃战：逃跑者ID
		8.1：A/B-S：“您逃离了跟B/A的战斗”
			[opcode][A][B]
		8.2：S-A/B：“A/B逃离了跟B/A的战斗”
			[opcode][A][B][RUNNER]
		8.3：S-B/A：“A/B逃离了本回合战斗，您赢了”
			[opcode][A][B][RUNNER]

客户--服务器
	包类型：需要的信息（客户：C；服务器：S）
	1.1 C-S查看当前在线列表：C的ID
request	[opcode=OP_CURR_USRLST_RQT][flags=0]
		[opr_cnt=0]
		1.1.1 “C请求查看在线列表”		--S
	1.2 S-C返回在线列表：在线列表
		[opcode=OP_CURR_USRLST_RPL][flags=0]
		[opr_cnt=number_of_returned_entries]
		[username]*multiple times

	2.1 C-S查看排行榜：C的ID（需要吗？）
request	[opcode=OP_RANKING_RQT][flags=0]
		[opr_cnt=0]
		2.1.1 “C请求查看排行榜”			--S
	2.2 S-C返回排行榜：排行榜
reply	[opcode=OP_RANKING_RPL][flags=0]
		[opr_cnt=number_of_returned_entries]
		[[username][score][ranking]] <- packed into a struct
		^ multiple times

	【3. 发送广播：C的ID、广播的话（这个功能要不要？）】

	4.1 C-S注册请求：C的ID、C的密码、C的邮箱（？）
request	[opcode=OP_REGISTER_RQT][flags=0][opr_cnt=3]
		[C-ID:length = 32][C-PWD:len=32][C-EM:len=64]
		4.1.1：“C请求注册XXX，密码XXX，邮箱XXX”	--S
	4.2 S-C注册回应：
success [opcode=OP_REGISTER_RPL][flags=FLAG_ACCEPT]
		[opr_cnt=0]
failure	[opcode=OP_REGISTER_RPL][flags=FLAG_REJECT]
		[opr_cnt=1][ERROR-CODE,refer to error number]
		4.2.1：“您请求注册的IDXXX注册失败，原因是XXX”	--C
		4.2.2：“IDXXX密码XXX注册成功，请登录”		--C

	5.1 C-S登录请求：C的ID、C的密码
request	[opcode=OP_LOGIN_RQT][flags=0][opr_cnt=2]
		[C-ID:len=32][C-PWD:len=32]
		5.1.1：“登录中...”			--C
	5.2 S-C登录回应：
success	[opcode=OP_LOGIN_RPL][flags=FLAG_ACCEPT]
		[opr_cnt=0]
failure	[opcode=OP_LOGIN_RPL][flags=FLAG_REJECT]
		[opr_cnt=1][ERROR-CODE, refer to error number]
		5.2.1：“登录失败，原因XXX”		--C
		5.2.2：“登录成功，原因XXX”		--C

