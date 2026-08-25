#include "pch.h"
#include "PinyinDict.h"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>

namespace Services
{
    namespace
    {
        // 词表：拼音 -> 候选词（前缀匹配用；同时按字拆出单字读音）
        struct WordEntry
        {
            const wchar_t* pinyin;
            const wchar_t* word;
        };

        // 常用词 + 游戏/软件名词库（精简版，覆盖日常输入与游戏库命名场景）
        constexpr WordEntry kWords[] = {
            {L"nihao", L"你好"}, {L"shijie", L"世界"}, {L"youxi", L"游戏"},
            {L"wanjia", L"玩家"}, {L"juese", L"角色"}, {L"renwu", L"任务"},
            {L"guanka", L"关卡"}, {L"zhandou", L"战斗"}, {L"maoxian", L"冒险"},
            {L"tansuo", L"探索"}, {L"gonglue", L"攻略"}, {L"cundang", L"存档"},
            {L"jindu", L"进度"}, {L"shezhi", L"设置"}, {L"xuanxiang", L"选项"},
            {L"yinyue", L"音乐"}, {L"huamian", L"画面"}, {L"caozuo", L"操作"},
            {L"shoubing", L"手柄"}, {L"jianpan", L"键盘"}, {L"shubiao", L"鼠标"},
            {L"pingmu", L"屏幕"}, {L"chuangkou", L"窗口"}, {L"caidan", L"菜单"},
            {L"kaishi", L"开始"}, {L"jieshu", L"结束"}, {L"zanting", L"暂停"},
            {L"jixu", L"继续"}, {L"tuichu", L"退出"}, {L"denglu", L"登录"},
            {L"zhuce", L"注册"}, {L"zhanghao", L"账号"}, {L"mima", L"密码"},
            {L"ziliao", L"资料"}, {L"shuju", L"数据"}, {L"wenjian", L"文件"},
            {L"tupian", L"图片"}, {L"shipin", L"视频"}, {L"xiazai", L"下载"},
            {L"shangchuan", L"上传"}, {L"anzhuang", L"安装"}, {L"xiezai", L"卸载"},
            {L"gengxin", L"更新"}, {L"banben", L"版本"}, {L"wangluo", L"网络"},
            {L"lianjie", L"连接"}, {L"fuwuqi", L"服务器"}, {L"shequ", L"社区"},
            {L"haoyou", L"好友"}, {L"liaotian", L"聊天"}, {L"xiaoxi", L"消息"},
            {L"tongzhi", L"通知"}, {L"xitong", L"系统"}, {L"yingyong", L"应用"},
            {L"chengxu", L"程序"}, {L"ruanjian", L"软件"}, {L"diannao", L"电脑"},
            {L"shouji", L"手机"}, {L"pingtai", L"平台"}, {L"zhichi", L"支持"},
            {L"bangzhu", L"帮助"}, {L"guanyu", L"关于"}, {L"fanhui", L"返回"},
            {L"queren", L"确认"}, {L"quxiao", L"取消"}, {L"shanchu", L"删除"},
            {L"tianjia", L"添加"}, {L"bianji", L"编辑"}, {L"baocun", L"保存"},
            {L"chuangjian", L"创建"}, {L"sousuo", L"搜索"}, {L"shuaixuan", L"筛选"},
            {L"paixu", L"排序"}, {L"shoucang", L"收藏"}, {L"biaoqian", L"标签"},
            {L"fenzu", L"分组"}, {L"liebiao", L"列表"}, {L"wangge", L"网格"},
            {L"xiangqing", L"详情"}, {L"shouye", L"首页"}, {L"daoru", L"导入"},
            {L"qidong", L"启动"}, {L"yunxing", L"运行"}, {L"dakai", L"打开"},
            {L"guanbi", L"关闭"}, {L"xuanze", L"选择"}, {L"quanbu", L"全部"},
            {L"yingxiong", L"英雄"}, {L"lianmeng", L"联盟"}, {L"chuanshuo", L"传说"},
            {L"wangguo", L"王国"}, {L"zhilei", L"之泪"}, {L"kuangye", L"旷野"},
            {L"zhixi", L"之息"}, {L"zhanshen", L"战神"}, {L"shiming", L"使命"},
            {L"zhaohuan", L"召唤"}, {L"xiandai", L"现代"}, {L"zhanzheng", L"战争"},
            {L"dipingxian", L"地平线"}, {L"jixian", L"极限"}, {L"jingsu", L"竞速"},
            {L"shenmi", L"神秘"}, {L"haiyu", L"海域"}, {L"zuihou", L"最后"},
            {L"shenghuanzhe", L"生还者"}, {L"wushi", L"巫师"}, {L"cike", L"刺客"},
            {L"xintiao", L"信条"}, {L"gudao", L"孤岛"}, {L"jinghun", L"惊魂"},
            {L"kandengou", L"看门狗"}, {L"siwang", L"死亡"}, {L"geqian", L"搁浅"},
            {L"aierdeng", L"艾尔登"}, {L"fahuan", L"法环"}, {L"heian", L"黑暗"},
            {L"zhihun", L"之魂"}, {L"zhilang", L"只狼"}, {L"guiqi", L"鬼泣"},
            {L"guaiwu", L"怪物"}, {L"lieren", L"猎人"}, {L"jueqi", L"崛起"},
            {L"taiyang", L"太阳"}, {L"diguo", L"帝国"}, {L"wenming", L"文明"},
            {L"quanzhan", L"全战"}, {L"sanguo", L"三国"}, {L"quanmian", L"全面"},
            {L"hongse", L"红色"}, {L"jingjie", L"警戒"}, {L"xingji", L"星际"},
            {L"zhengba", L"争霸"}, {L"moshou", L"魔兽"}, {L"lushi", L"炉石"},
            {L"shouwang", L"守望"}, {L"xianfeng", L"先锋"}, {L"juedi", L"绝地"},
            {L"qiusheng", L"求生"}, {L"wangzhe", L"王者"}, {L"rongyao", L"荣耀"},
            {L"heping", L"和平"}, {L"jingying", L"精英"}, {L"diwu", L"第五"},
            {L"renge", L"人格"}, {L"mingri", L"明日"}, {L"zhihou", L"之后"},
            {L"yongjie", L"永劫"}, {L"wujian", L"无间"}, {L"tianya", L"天涯"},
            {L"mingyue", L"明月"}, {L"mengxiang", L"梦想"}, {L"heise", L"黑色"},
            {L"shamo", L"沙漠"}, {L"shiluo", L"失落"}, {L"fangzhou", L"方舟"},
            {L"huanta", L"幻塔"}, {L"danzi", L"蛋仔"}, {L"paidui", L"派对"},
            {L"yuanmeng", L"元梦"}, {L"benghe", L"崩坏"}, {L"xingqiong", L"星穹"},
            {L"tiedao", L"铁道"}, {L"juequ", L"绝区"}, {L"mingchao", L"鸣潮"},
            {L"xingdong", L"行动"}, {L"anqu", L"暗区"}, {L"tuwei", L"突围"},
            {L"wuwei", L"无畏"}, {L"qiyue", L"契约"}, {L"huojian", L"火箭"},
            {L"baolei", L"堡垒"}, {L"zhive", L"之夜"}, {L"wode", L"我的"},
            {L"taila", L"泰拉"}, {L"ruiya", L"瑞亚"}, {L"xinglou", L"星露"},
            {L"guwu", L"谷物"}, {L"kongdong", L"空洞"}, {L"qishi", L"骑士"},
            {L"hadisi", L"哈迪斯"}, {L"yisha", L"以撒"}, {L"jiehe", L"结合"},
            {L"weilan", L"蔚蓝"}, {L"chabei", L"茶杯"}, {L"manyou", L"漫游"},
            {L"zhiming", L"致命"}, {L"gongsi", L"公司"}, {L"qianshui", L"潜水"},
            {L"daisen", L"戴森"}, {L"yixing", L"异星"}, {L"gongchang", L"工厂"},
            {L"chengshi", L"城市"}, {L"tianjixian", L"天际线"}, {L"moni", L"模拟"},
            {L"nongchang", L"农场"}, {L"ouzhou", L"欧洲"}, {L"kache", L"卡车"},
            {L"bashi", L"巴士"}, {L"weiruan", L"微软"}, {L"feixing", L"飞行"},
            {L"weixin", L"微信"}, {L"dingding", L"钉钉"}, {L"youdao", L"有道"},
            {L"cidian", L"词典"}, {L"yuanshen", L"原神"}, {L"baidu", L"百度"},
            {L"wangpan", L"网盘"}, {L"zhifubao", L"支付宝"}, {L"taobao", L"淘宝"},
            {L"jingdong", L"京东"}, {L"meituan", L"美团"}, {L"douyin", L"抖音"},
            {L"kuaishou", L"快手"}, {L"zhihu", L"知乎"}, {L"weibo", L"微博"},
            {L"biji", L"笔记"}, {L"saierda", L"塞尔达"}, {L"chuanqi", L"传奇"},
            {L"xianjian", L"仙剑"}, {L"wangyou", L"网游"}, {L"danji", L"单机"},
            {L"shuangren", L"双人"}, {L"chengjiu", L"成就"}, {L"renleibing", L"人类"},
        };

        // 单字补充：拼音 -> 常用字（多音字取常用读音）
        struct CharEntry
        {
            const wchar_t* pinyin;
            const wchar_t* chars;
        };

        constexpr CharEntry kChars[] = {
            {L"ai", L"爱哀唉矮挨碍癌艾"}, {L"an", L"安按暗岸案俺鞍"},
            {L"ba", L"把吧八巴爸罢霸坝"}, {L"bai", L"白百败摆拜柏"},
            {L"ban", L"半办班般板版伴搬拌"}, {L"bang", L"帮棒榜邦绑膀"},
            {L"bao", L"报包保宝饱暴抱爆薄"}, {L"bei", L"被北备背杯悲贝辈"},
            {L"ben", L"本奔笨苯"}, {L"bi", L"比必笔毕闭币避逼鼻"},
            {L"bian", L"变边便编遍扁辨辩"}, {L"biao", L"表标票彪"},
            {L"bie", L"别憋撇"}, {L"bing", L"并病兵冰丙饼"},
            {L"bo", L"波播伯博脖剥玻"}, {L"bu", L"不步部补布捕堡"},
            {L"cai", L"才采菜财彩猜踩"}, {L"can", L"参残餐惨蚕灿"},
            {L"cang", L"藏仓苍舱"}, {L"cao", L"草操曹槽糙"},
            {L"ce", L"册测侧策厕"}, {L"ceng", L"层曾蹭"},
            {L"cha", L"查差茶插察叉"}, {L"chang", L"长常场厂唱尝肠"},
            {L"chao", L"超抄朝潮吵炒"}, {L"che", L"车彻撤扯"},
            {L"chen", L"陈晨沉尘臣衬"}, {L"cheng", L"成城程称乘诚承"},
            {L"chi", L"吃持池迟齿尺翅赤"}, {L"chong", L"冲重充虫崇"},
            {L"chou", L"抽愁仇臭丑绸"}, {L"chu", L"出初除处厨楚触"},
            {L"chuan", L"穿船传串川"}, {L"chuang", L"窗床创闯"},
            {L"chun", L"春纯唇蠢"}, {L"ci", L"次词此磁刺辞"},
            {L"cong", L"从聪丛葱"}, {L"cu", L"粗促醋"},
            {L"cun", L"村存寸"}, {L"cuo", L"错措搓"},
            {L"da", L"大打达答搭嗒"}, {L"dai", L"带代待戴袋呆"},
            {L"dan", L"但单蛋担淡弹胆"}, {L"dang", L"当党挡荡"},
            {L"dao", L"到道倒刀岛导盗"}, {L"de", L"的得德"},
            {L"deng", L"等灯登瞪"}, {L"di", L"地第低底敌帝滴"},
            {L"dian", L"点电店典垫殿"}, {L"diao", L"掉调吊钓"},
            {L"die", L"爹跌叠碟"}, {L"ding", L"定顶丁盯钉"},
            {L"dong", L"动东冬懂洞冻"}, {L"dou", L"都斗豆抖逗"},
            {L"du", L"读独度肚毒堵"}, {L"duan", L"段短断端锻"},
            {L"dui", L"对队堆"}, {L"dun", L"顿蹲盾吨"},
            {L"duo", L"多夺朵躲舵"}, {L"e", L"饿恶额鹅俄"},
            {L"en", L"恩"}, {L"er", L"二而儿耳尔"},
            {L"fa", L"发法罚乏伐"}, {L"fan", L"反饭翻烦凡返范"},
            {L"fang", L"方放房防访仿芳"}, {L"fei", L"非飞费肥肺废"},
            {L"fen", L"分份粉愤奋纷"}, {L"feng", L"风封峰疯丰枫"},
            {L"fu", L"服复父付富福负夫府"}, {L"gai", L"改该盖概"},
            {L"gan", L"干感赶敢甘肝"}, {L"gang", L"刚钢港岗"},
            {L"gao", L"高告搞稿糕"}, {L"ge", L"个各歌格哥隔"},
            {L"gei", L"给"}, {L"gen", L"跟根"},
            {L"geng", L"更耕"}, {L"gong", L"工公共功攻供宫"},
            {L"gou", L"够狗构购沟"}, {L"gu", L"古故顾股骨姑"},
            {L"gua", L"挂瓜刮"}, {L"guai", L"怪拐"},
            {L"guan", L"关管观官馆惯冠"}, {L"guang", L"光广"},
            {L"gui", L"规归鬼贵柜跪"}, {L"guo", L"过国果锅"},
            {L"ha", L"哈"}, {L"hai", L"还海害孩"},
            {L"han", L"汉含寒喊汗旱"},
            {L"hang", L"行航杭"}, {L"hao", L"好号豪耗浩"},
            {L"he", L"和何合河喝荷盒"}, {L"hei", L"黑"},
            {L"hen", L"很恨狠"}, {L"heng", L"横恒"},
            {L"hong", L"红宏洪轰"}, {L"hou", L"后候厚猴"},
            {L"hu", L"户护湖虎胡呼忽"}, {L"hua", L"话花华画化滑"},
            {L"huai", L"坏怀"}, {L"huan", L"换还欢环缓患"},
            {L"huang", L"黄皇慌晃"}, {L"hui", L"回会灰挥恢悔毁"},
            {L"hun", L"婚混昏魂"}, {L"huo", L"活火或获货"},
            {L"ji", L"几机级记计及急极集既"}, {L"jia", L"家加甲假架价佳"},
            {L"jian", L"见件间建简剑健减检"}, {L"jiang", L"将讲江降奖"},
            {L"jiao", L"叫教交角脚觉较校"}, {L"jie", L"接解界节结借姐"},
            {L"jin", L"进金今近紧尽仅禁"}, {L"jing", L"经京静竟精景境"},
            {L"jiu", L"就九久酒旧救究"}, {L"ju", L"具局举句距剧居"},
            {L"juan", L"卷捐"}, {L"jue", L"决觉绝角"},
            {L"jun", L"军均君"}, {L"ka", L"卡"},
            {L"kai", L"开凯"}, {L"kan", L"看砍刊"},
            {L"kang", L"康抗"}, {L"kao", L"考靠"},
            {L"ke", L"可科客课克刻"}, {L"ken", L"肯"},
            {L"kong", L"空控恐"}, {L"kou", L"口扣"},
            {L"ku", L"哭库苦裤"}, {L"kuai", L"快块筷"},
            {L"kuan", L"宽款"}, {L"kuang", L"况框狂"},
            {L"kui", L"亏愧"}, {L"kun", L"困"},
            {L"kuo", L"扩阔"}, {L"la", L"拉啦辣"},
            {L"lai", L"来赖"}, {L"lan", L"蓝栏懒烂兰"},
            {L"lang", L"浪郎狼"}, {L"lao", L"老劳牢"},
            {L"le", L"了乐"}, {L"lei", L"类泪累雷"},
            {L"leng", L"冷"}, {L"li", L"里力立利例离理礼"},
            {L"lian", L"连联练脸恋链"}, {L"liang", L"两量亮良凉"},
            {L"liao", L"了料聊"}, {L"lie", L"列烈裂"},
            {L"lin", L"林临邻"}, {L"ling", L"零灵领令"},
            {L"liu", L"六流留刘"}, {L"long", L"龙隆"},
            {L"lou", L"楼漏"}, {L"lu", L"路录陆露炉"},
            {L"lv", L"绿律旅"}, {L"luan", L"乱"},
            {L"lun", L"论轮"}, {L"luo", L"落罗"},
            {L"ma", L"吗马妈码麻"}, {L"mai", L"买卖麦"},
            {L"man", L"满慢漫"}, {L"mang", L"忙"},
            {L"mao", L"毛冒帽猫"}, {L"me", L"么"},
            {L"mei", L"没每美妹梅"}, {L"men", L"们门闷"},
            {L"meng", L"梦猛蒙"}, {L"mi", L"米密迷秘"},
            {L"mian", L"面免棉"}, {L"miao", L"秒苗妙"},
            {L"min", L"民敏"}, {L"ming", L"名明命鸣"},
            {L"mo", L"莫末摸磨"}, {L"mou", L"某"},
            {L"mu", L"目木母幕"}, {L"na", L"那拿哪"},
            {L"nai", L"乃奶"}, {L"nan", L"难男南"},
            {L"nao", L"脑闹"}, {L"ne", L"呢"},
            {L"nei", L"内"}, {L"neng", L"能"},
            {L"ni", L"你尼泥"}, {L"nian", L"年念"},
            {L"niao", L"鸟"}, {L"ning", L"宁"},
            {L"niu", L"牛扭"}, {L"nong", L"农弄浓"},
            {L"nv", L"女"}, {L"ou", L"欧偶"},
            {L"pa", L"怕爬"}, {L"pai", L"拍排派"},
            {L"pan", L"盘盼判"}, {L"pang", L"旁胖"},
            {L"pao", L"跑泡"}, {L"pei", L"陪配培"},
            {L"peng", L"朋碰"}, {L"pi", L"批皮匹"},
            {L"pian", L"片偏骗"}, {L"piao", L"票飘"},
            {L"pin", L"品频"}, {L"ping", L"平评瓶"},
            {L"po", L"破婆"}, {L"pu", L"普朴"},
            {L"qi", L"起七其期气齐器"}, {L"qian", L"前钱千签浅欠"},
            {L"qiang", L"强枪墙"}, {L"qiao", L"桥巧"},
            {L"qie", L"切且"}, {L"qin", L"亲勤琴"},
            {L"qing", L"请青情清轻庆"}, {L"qiu", L"求球秋"},
            {L"qu", L"去取区曲趣"}, {L"quan", L"全权圈劝"},
            {L"que", L"却缺确"}, {L"qun", L"群"},
            {L"ran", L"然染"}, {L"rang", L"让"},
            {L"rao", L"绕"}, {L"re", L"热"},
            {L"ren", L"人认任忍仁"}, {L"ri", L"日"},
            {L"rong", L"容荣"}, {L"rou", L"肉柔"},
            {L"ru", L"如入"}, {L"ruan", L"软"},
            {L"rui", L"瑞"}, {L"run", L"润"},
            {L"ruo", L"若弱"}, {L"sa", L"洒"},
            {L"sai", L"赛塞"}, {L"san", L"三散伞"},
            {L"sao", L"扫"}, {L"se", L"色"},
            {L"sen", L"森"}, {L"sha", L"杀沙傻"},
            {L"shan", L"山善闪删"}, {L"shang", L"上商伤赏"},
            {L"shao", L"少烧"}, {L"she", L"社设蛇舍"},
            {L"shen", L"什深身神"}, {L"sheng", L"生声升省胜"},
            {L"shi", L"是十时事使世实石师市"}, {L"shou", L"手受收首售"},
            {L"shu", L"书数术树属输熟舒"}, {L"shuang", L"双"},
            {L"shui", L"水谁睡"}, {L"shun", L"顺"},
            {L"shuo", L"说"}, {L"si", L"四死思私似丝"},
            {L"song", L"送松"}, {L"sou", L"搜"},
            {L"su", L"素速苏诉"}, {L"suan", L"算酸"},
            {L"sui", L"随岁碎虽"}, {L"sun", L"孙损"},
            {L"suo", L"所锁"}, {L"ta", L"他她它"},
            {L"tai", L"太台态"}, {L"tan", L"谈弹叹"},
            {L"tang", L"汤堂糖"}, {L"tao", L"讨逃套"},
            {L"te", L"特"}, {L"teng", L"疼"},
            {L"ti", L"提题体替"}, {L"tian", L"天田填甜"},
            {L"tiao", L"条跳调"}, {L"tie", L"铁贴"},
            {L"ting", L"听停庭"}, {L"tong", L"同通痛铜统"},
            {L"tou", L"头投透"}, {L"tu", L"图土突途"},
            {L"tuan", L"团"}, {L"tui", L"推退腿"},
            {L"tuo", L"脱拖托"}, {L"wa", L"瓦挖"},
            {L"wai", L"外歪"}, {L"wan", L"完晚万玩"},
            {L"wang", L"王往望忘网"}, {L"wei", L"为位未微围卫委"},
            {L"wen", L"问文温闻稳"}, {L"wo", L"我握"},
            {L"wu", L"五无物务误屋武"}, {L"xi", L"系细西息希喜习戏"},
            {L"xia", L"下夏吓"}, {L"xian", L"现先线显险县"},
            {L"xiang", L"想向相象香响项乡"}, {L"xiao", L"小笑消校效"},
            {L"xie", L"写些谢协血鞋"}, {L"xin", L"心新信辛"},
            {L"xing", L"行型形兴姓幸性星"}, {L"xiu", L"修秀休"},
            {L"xu", L"需许续徐序"}, {L"xuan", L"选宣悬"},
            {L"xue", L"学雪血"}, {L"xun", L"寻训讯"},
            {L"ya", L"呀压牙雅"}, {L"yan", L"眼言沿研严颜演"},
            {L"yang", L"样阳养羊扬洋"}, {L"yao", L"要药摇咬邀"},
            {L"ye", L"也业夜叶"}, {L"yi", L"一以已意义医易衣"},
            {L"yin", L"因音银引印"}, {L"ying", L"应影英营赢"},
            {L"yong", L"用永勇拥"}, {L"you", L"有又由友右游优油"},
            {L"yu", L"于与语雨玉遇余鱼"}, {L"yuan", L"远元原员圆院愿"},
            {L"yue", L"月越乐约阅"}, {L"yun", L"云运允"},
            {L"zai", L"在再载灾"}, {L"zan", L"赞"},
            {L"zao", L"早造"}, {L"ze", L"则责"},
            {L"zen", L"怎"}, {L"zeng", L"增"},
            {L"zha", L"炸扎"}, {L"zhai", L"摘窄"},
            {L"zhan", L"站战展占"}, {L"zhang", L"张长章掌"},
            {L"zhao", L"找照招朝"}, {L"zhe", L"这着者折"},
            {L"zhen", L"真针震珍"}, {L"zheng", L"正证争整政"},
            {L"zhi", L"之只知指直至制治志值"}, {L"zhong", L"中种重众终"},
            {L"zhou", L"周州"}, {L"zhu", L"主住助注竹猪"},
            {L"zhuan", L"专转"}, {L"zhuang", L"装状壮庄"},
            {L"zhui", L"追"}, {L"zhun", L"准"},
            {L"zhuo", L"桌捉"}, {L"zi", L"子自字资紫"},
            {L"zong", L"总纵"}, {L"zou", L"走奏"},
            {L"zu", L"组足祖族"}, {L"zui", L"最嘴罪"},
            {L"zun", L"尊"}, {L"zuo", L"做作坐座左"},
        };

        struct Tables
        {
            std::unordered_map<std::wstring, std::vector<std::wstring>> words;
            std::unordered_map<std::wstring, std::vector<std::wstring>> chars;
        };

        Tables const& Build()
        {
            static Tables t;
            static bool built = false;
            if (!built)
            {
                for (auto const& e : kWords)
                {
                    std::wstring py(e.pinyin);
                    auto& arr = t.words[py];
                    std::wstring w(e.word);
                    if (std::find(arr.begin(), arr.end(), w) == arr.end())
                    {
                        arr.push_back(w);
                    }
                }
                for (auto const& e : kChars)
                {
                    std::wstring py(e.pinyin);
                    auto& arr = t.chars[py];
                    for (auto c : std::wstring(e.chars))
                    {
                        std::wstring s(1, c);
                        if (std::find(arr.begin(), arr.end(), s) == arr.end())
                        {
                            arr.push_back(s);
                        }
                    }
                }
                built = true;
            }
            return t;
        }
    }

    std::vector<std::wstring> PinyinDict::GetCandidates(std::wstring const& buffer)
    {
        std::wstring b = buffer;
        // 去掉空白并小写
        std::wstring clean;
        for (auto c : b)
        {
            if (c == L' ')
            {
                continue;
            }
            if (c >= L'A' && c <= L'Z')
            {
                c = static_cast<wchar_t>(c + (L'a' - L'A'));
            }
            clean.push_back(c);
        }
        if (clean.empty())
        {
            return {};
        }

        auto const& t = Build();
        std::vector<std::wstring> out;

        // 词表前缀匹配优先
        for (auto const& [py, words] : t.words)
        {
            if (py.compare(0, clean.size(), clean) == 0)
            {
                out.insert(out.end(), words.begin(), words.end());
            }
        }
        // 单字前缀匹配
        for (auto const& [py, chars] : t.chars)
        {
            if (py.compare(0, clean.size(), clean) == 0)
            {
                out.insert(out.end(), chars.begin(), chars.end());
            }
        }

        std::unordered_set<std::wstring> seen;
        std::vector<std::wstring> result;
        for (auto const& item : out)
        {
            if (seen.insert(item).second)
            {
                result.push_back(item);
            }
        }
        if (result.size() > 24)
        {
            result.resize(24);
        }
        return result;
    }
}