客户--客户
==============
包类型：需要的信息（挑战者：A；被挑战者：B）
--------------
<br />
###挑战请求（A挑战B）：【A->S】【S->B】
```
[opcode=OP_CHALLENGE_RQT]
[flags=0]
[opr_cnt=4]
[id = A-ID]
[id = B-ID]
[name = A-username]
[name = B-username]
```
#### 【A】：“您向B发起了挑战，等待B响应...”
#### 【S】：“A向B发起了挑战”
#### 【B】：“A向您发起了挑战，是否接受？”
 <br />  
###挑战回应（B拒绝A的挑战）：【B->S】【S->A】
```
[opcode=OP_CHALLENGE_RPL]
[flags=FLAG_REJECT]
[opr_cnt=2]
[id = A-ID]
[id = B-ID]
```
#### 【A】：“B拒绝了您的挑战”
#### 【S】：“B拒绝了A的挑战”
#### 【B】：“您拒绝了A的挑战”
 <br />  
###挑战回应（B接受A的挑战）：【B->S】【S->A】
```
[opcode=OP_CHALLENGE_RPL]
[flags=FLAG_ACCEPT]
[opr_cnt=2]
[id = A-ID]
[id = A-ID]
```
#### 【A】：“B接受了您的挑战，战斗开始”
#### 【S】：“B接受了A的挑战，AB对战开始”
#### 【B】：“您接受了B的挑战，战斗开始”
 <br />  
###战斗输入1（A挑战B，B接受挑战）：【A->S】
```
[opcode=OP_BATTLE_INPUT]
[flags=0]
[opr_cnt=2]
[A-ID]
[A-input]
```
#### 【A】：“您输入XXX，等待B的输入”
###战斗输入2（A挑战B，B接受挑战）：【B->S】
```
[opcode=OP_BATTLE_INPUT]
[flags=0]
[opr_cnt=2]
[B-ID]
[B-input]
```
#### 【S】：“A输入XXX，B输入XXX，战斗开始”
 <br />  
###战斗开始（A挑战B，B接受挑战）：【S->A】【S->B】
```
[opcode= OP_BATTLE_MSG ]
[flags= FLAG_BATTLE_INTRO]
[opr_cnt=4]
[A-ID]
[A-input]
[B-ID]
[B-input]
```
#### 【S】：“A输入XXX，B输入XXX，战斗开始”
#### 【A】：“您输入XXX，B输入XXX，战斗开始”
#### 【B】：“A输入XXX，您输入XXX，战斗开始”
 <br />  
###战斗过程（A挑战B）：
```
[opcode= OP_BATTLE_MSG]
[flags= FLAG_BATTLE_EACH]
[opr_cnt=5]
[userid]
[move_name]
[userid]
[move_name]
[winner-id]
```
##### winner-id: 0 - 前者赢， 1 - 平手， 2 - 后者赢
#### 【S】：“A出XX，B出XX，结果XX”
#### 【A】：“本局您出XXX，B出XXX，结果XX”
#### 【B】：“本局A出XXX，您出XXX，结果XX”
 <br />  
###战斗结束：A的ID&结果、B的ID&结果
```
[opcode=OP_BATTLE_RESULT]
[flags=0]
[opr_cnt=8]
[user_id]
[win]
[lose]
[draw]
[user_id]
[win]
[lose]
[draw]
```
#### 【S】：“A对战B，A胜X局，B胜Y局，平Z局，A/B获胜”
#### 【A】：“您挑战B，胜X局，负Y局，平Z局，您赢/输了”
#### 【B】：“A挑战您，胜X局，负Y局，平Z局，您赢/输了”


客户--服务器
===============
包类型：需要的信息（客户：C；服务器：S）
---------------
### 查看当前在线列表：
####request
```
[opcode=OP_CURR_USRLST_RQT]
[flags=0]
[opr_cnt=0]
```
####reply
```
[opcode=OP_CURR_USRLST_RPL]
[flags=0]
[opr_cnt=number_of_returned_entries]
[[id][gamingstatus][username]]
		^ multiple times
```
 <br />  
###查看排行榜
####request
```
[opcode=OP_RANKING_RQT]
[flags=0]
[opr_cnt=0]
```
####reply
```
[opcode=OP_RANKING_RPL]
[flags=0]
[opr_cnt=number_of_returned_entries]
[[id][username][score][ranking]] <- packed into a struct
		^ multiple times
```
 <br />  
###注册
####request
```
[opcode=OP_REGISTER_RQT]
[flags=0]
[opr_cnt=3]
[C-ID:length = 32]
[C-PWD:len=32]
[C-EM:len=64]
```
####reply
#####success
```
[opcode=OP_REGISTER_RPL]
[flags=FLAG_ACCEPT]
[opr_cnt=1]
[0]
```
#####failure
```
[opcode=OP_REGISTER_RPL]
[flags=FLAG_REJECT]
[opr_cnt=1]
[ERROR-CODE,refer to error number]
```
 <br />  
###登录
####request
```
[opcode=OP_LOGIN_RQT]
[flags=0]
[opr_cnt=2]
[C-ID:len=32]
[C-PWD:len=32]
```
####reply
#####success
```
[opcode=OP_LOGIN_RPL]
[flags=FLAG_ACCEPT]
[opr_cnt=1]
[userinfo]
```
#####failure
```
[opcode=OP_LOGIN_RPL]
[flags=FLAG_REJECT]
[opr_cnt=1]
[ERROR-CODE, refer to error number]
```
 <br />  

