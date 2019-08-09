using System;
using System.Windows.Forms;
using System.Text;
using System.Net;
using System.Net.Sockets;
using System.Linq;
using System.Drawing;
using System.IO;
using System.IO.Ports;
using System.Threading;
using System.Threading.Tasks;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;

namespace NewTestWheel
{
    public partial class Form1 : Form
    {
        string[] ui_speed = new string[11] {"0", "1.13", "1.63", "3.01", "3.36", "4.51", "5.05", "6.27", "6.77", "8.04", "8.67"};
        byte[] recvBuffer = new byte[64];
        byte[] dataBuffer = new byte[100];
        byte[] sendBuffer = new byte[6] { 114, 100, 0, 0, 0, 0 };
/* magic number of speed should be done with macro */       ushort[] DACtable = new ushort[11] { 0x0000, 0x0174, 0x02E8, 0x045D, 0x5D1, 0x0745, 0x08B9, 0x0A2D, 0x0BA2, 0x0D16, 0x0E8A };// [1] was 0x00F8        //ushort[] DACtable = new ushort[6] { 0x0, 0x020d, 0x041a, 0x0521, 0x0628 };
        byte[] DACspeed = new byte[6] { 0, 0, 0, 0, 0, 0 };
        StreamWriter resultStreamWriter;
        SerialPort serialPort = new SerialPort();
        List<Byte> receiveDataList = new List<Byte>();
        char[] knockDoorConst = new char[7] { 'R', 'D', 'Y','\0', '\0', '\0', '\0' };
        byte timeoutCount = 0;
        Module_Info arm_Info;
        long EndTimeStamp;
        /* Freeway mode related */
        uint Freeway_timerCount = 0;
        double round_L = 0;
        double round_M = 0;
        double round_R = 0;
        const double wheelDiameter = 52.0;
        /*----------------------*/

        /* Successive mode related */
        UInt16 successive_L_tv, idx_L;
        UInt16 successive_M_tv, idx_M;
        UInt16 successive_R_tv, idx_R;
        UInt16 successive_time_elapsed = 0;
        long start_timestamp;
        /*-------------------------*/

        public Form1()
        {
            InitializeComponent();
        }

        private void Form1_Load(object sender, EventArgs e)//表單一進來跑的地方(前置作業)//除錯
        {
            int TC = (int)TimeConsuming.Value;
            string[] ports = SerialPort.GetPortNames();
            SerialPortSelect.Items.AddRange(ports);
            GlobalBuffer.g_isThreadWorking = false;
            arm_Info.trainState = TrainingStatus.STANDBY;//Debug
            arm_Info.netState = ConnectionStatus.CONNECTED_KNOCK_DOOR; // CHANGEED FOR UART TRANSMISSION //Debug
            arm_Info.isDataReceived = false;
            GlobalBuffer.g_dataNeedProcess = false;
            NetworkTimer.Interval = 100;//Debug時間
            Data();
            RatLeftAngle.BackColor = Color.DarkGray;
            RatMiddleAngle.BackColor = Color.DarkGray;
            RatRightAngle.BackColor = Color.DarkGray;
            RatLeftSpeed.BackColor = Color.Gainsboro;
            RatMiddleSpeed.BackColor = Color.Gainsboro;
            RatRightSpeed.BackColor = Color.Gainsboro;
        }

        public enum ConnectionStatus
        {
            UNCONNECT = 1,
            CONNECTED,
            CONNECTED_KNOCK_DOOR,
            CONNECTED_KNOCK_DOOR_WAIT, // used in uart mode 
            END_TRAINING_IN_PROGRESS, // used in uart mode 
            END_TRAINING_WAIT_PROGRESS, // used in uart mode 
            TRAINING_END // used in uart mode 
        }
        public enum MazeStatus
        {
            WAIT_FOR_RAT = 1,
            RAT_NOT_ENTERED,
            RAT_ENTERED,
            TRAINING_END
        }
        public enum TrainingStatus
        {
            STANDBY = 6,
            RUNNING,
            COMPLETE
        }
        public struct Module_Info//Debug
        {
            public short[] shortTermError;
            public short[] longTermError;
            public short food_left;
            public TrainingStatus trainState;
            public ConnectionStatus netState;
            public bool isDataReceived; // this flag determine wheather receive the incoming data 
        }
        static class GlobalBuffer
        {
            public static byte[] g_recvBuffer = new byte[64];
            public static bool g_isDataReceive;
            public static bool g_dataNeedProcess;
            public static bool g_isThreadWorking;

        }
        static public long GetCurrentTimestamp()
        {
            return (long)(DateTime.UtcNow - new DateTime(1970, 1, 1, 0, 0, 0, 0)).TotalSeconds;
        }
        public void Data()//儲存SensingPoint所有資料
        {
            //Store the values of one to five individual items for individual sensing points on the left machine.
            SensingPoint LR = new SensingPoint(11, "SSLR", (int)SpeedSettingLR.Value, (int)AngleSettingLR.Value);
            label5_17.Text = LR.Angle + "°";
            SensingPoint LY = new SensingPoint(12, "SSLY", (int)SpeedSettingLY.Value, (int)AngleSettingLY.Value);
            label5_27.Text = LY.Angle + "°";
            SensingPoint LG = new SensingPoint(13, "SSLG", (int)SpeedSettingLG.Value, (int)AngleSettingLG.Value);
            label5_37.Text = LG.Angle + "°";
            SensingPoint LB = new SensingPoint(14, "SSLB", (int)SpeedSettingLB.Value, (int)AngleSettingLB.Value);
            label5_47.Text = LB.Angle + "°";
            SensingPoint LP = new SensingPoint(15, "SSLP", (int)SpeedSettingLP.Value, (int)AngleSettingLP.Value);
            label5_57.Text = LP.Angle + "°";
            //Store the values of the individual sensing points of the intermediate machine from one to five individual items.
            SensingPoint MR = new SensingPoint(21, "SSMR", (int)SpeedSettingMR.Value, (int)AngleSettingMR.Value);
            label6_17.Text = MR.Angle + "°";
            SensingPoint MY = new SensingPoint(22, "SSMY", (int)SpeedSettingMY.Value, (int)AngleSettingMY.Value);
            label6_27.Text = MY.Angle + "°";
            SensingPoint MG = new SensingPoint(23, "SSMG", (int)SpeedSettingMG.Value, (int)AngleSettingMG.Value);
            label6_37.Text = MG.Angle + "°";
            SensingPoint MB = new SensingPoint(24, "SSMB", (int)SpeedSettingMB.Value, (int)AngleSettingMB.Value);
            label6_47.Text = MB.Angle + "°";
            SensingPoint MP = new SensingPoint(25, "SSMP", (int)SpeedSettingMP.Value, (int)AngleSettingMP.Value);
            label6_57.Text = MP.Angle + "°";
            //Store the values of the individual sensing points of the right machine from one to five individual items.
            SensingPoint RR = new SensingPoint(31, "SSRR", (int)SpeedSettingRR.Value, (int)AngleSettingRR.Value);
            label7_17.Text = RR.Angle + "°";
            SensingPoint RY = new SensingPoint(32, "SSRY", (int)SpeedSettingRY.Value, (int)AngleSettingRY.Value);
            label7_27.Text = RY.Angle + "°";
            SensingPoint RG = new SensingPoint(33, "SSRG", (int)SpeedSettingRG.Value, (int)AngleSettingRG.Value);
            label7_37.Text = RG.Angle + "°";
            SensingPoint RB = new SensingPoint(34, "SSRB", (int)SpeedSettingRB.Value, (int)AngleSettingRB.Value);
            label7_47.Text = RB.Angle + "°";
            SensingPoint RP = new SensingPoint(35, "SSRP", (int)SpeedSettingRP.Value, (int)AngleSettingRP.Value);
            label7_57.Text = RP.Angle + "°";
        }
        //滾輪類 左側 角度組
        private void AngleSettingLR_Scroll(object sender, EventArgs e)
        {
            label5_17.Text = AngleSettingLR.Value + "°";
        }
        private void AngleSettingLY_Scroll(object sender, EventArgs e)
        {
            label5_27.Text = AngleSettingLY.Value + "°";
        }
        private void AngleSettingLG_Scroll(object sender, EventArgs e)
        {
            label5_37.Text = "90°";
        }
        private void AngleSettingLB_Scroll(object sender, EventArgs e)
        {
            label5_47.Text = AngleSettingLB.Value + "°";
        }
        private void AngleSettingLP_Scroll(object sender, EventArgs e)
        {
            label5_57.Text = AngleSettingLP.Value + "°";
        }
        //滾輪類 中間 角度組
        private void AngleSettingMR_Scroll(object sender, EventArgs e)
        {
            label6_17.Text = AngleSettingMR.Value + "°";
        }
        private void AngleSettingMY_Scroll(object sender, EventArgs e)
        {
            label6_27.Text = AngleSettingMY.Value + "°";
        }
        private void AngleSettingMG_Scroll(object sender, EventArgs e)
        {
            label6_37.Text = "90°";
        }
        private void AngleSettingMB_Scroll(object sender, EventArgs e)
        {
            label6_47.Text = AngleSettingMB.Value + "°";
        }
        private void AngleSettingMP_Scroll(object sender, EventArgs e)
        {
            label6_57.Text = AngleSettingMP.Value + "°";
        }
        //滾輪類 右側 角度組
        private void AngleSettingRR_Scroll(object sender, EventArgs e)
        {
            label7_17.Text = AngleSettingRR.Value + "°";
        }
        private void AngleSettingRY_Scroll(object sender, EventArgs e)
        {
            label7_27.Text = AngleSettingRY.Value + "°";
        }
        private void AngleSettingRG_Scroll(object sender, EventArgs e)
        {
            label7_37.Text = "90°";
        }
        private void AngleSettingRB_Scroll(object sender, EventArgs e)
        {
            label7_47.Text = AngleSettingRB.Value + "°";
        }
        private void AngleSettingRP_Scroll(object sender, EventArgs e)
        {
            label7_57.Text = AngleSettingRP.Value + "°";
        }

        private void ResultFileDialog_FileOk(object sender, CancelEventArgs e)//檔案類
        {
            ResultFilePath.ForeColor = Color.Black;
            ResultFilePath.Text = ResultFileDialog.FileName;
        }

        private void TimeAccumulationTimer_Tick(object sender, EventArgs e)// max counting time should under 1 hour //時間計數
        {
            TimeLeft.Text = ((EndTimeStamp - GetCurrentTimestamp()) / 60).ToString() + " : " + ((EndTimeStamp - GetCurrentTimestamp()) % 60).ToString();//時間計數
        }
        private void NetworkTimer_Tick(object sender, EventArgs e)
        {
            switch (arm_Info.netState)
            {
                case ConnectionStatus.CONNECTED_KNOCK_DOOR:
                    if (!Freeway_mode.Checked && !Successive_mode.Checked) // this is normal mode
                        serialPort.Write(knockDoorConst, 0, 7);
                    else if (Freeway_mode.Checked)
                        serialPort.Write(new byte[7] { 0x88, (byte)'D', (byte)'Y' ,0,0,0,0}, 0, 7); // we represent mode with bit
                    else if (Successive_mode.Checked)
                    {
                        UInt16 total = (UInt16)TimeConsuming.Value;
                        total *= 60;

                        // note that this mechanism cause deviation of interval, which has maximum -> (max speed level - 1) seconds
                        UInt16 left_tv = (UInt16) (total /  Successive_speed_lv_L.Value);
                        UInt16 mid_tv = (UInt16) (total /  Successive_speed_lv_M.Value);
                        UInt16 right_tv = (UInt16) (total / Successive_speed_lv_R.Value);

                        // used to update speed display at UI
                        successive_L_tv = left_tv;
                        successive_M_tv = mid_tv;
                        successive_R_tv = right_tv;

                        // the first byte is mode (successive mode), [1] is low byte, whereas [2] is high byte. then you can know the preceeding ones
                        serialPort.Write(new byte[7] { 0x48, (byte)(left_tv & 0xff), (byte)((left_tv >> 8) & 0xff), (byte)(mid_tv & 0xff), (byte)((mid_tv >> 8) & 0xff), (byte)(right_tv & 0xff), (byte)((right_tv >> 8) & 0xff) }, 0, 7);
                        Successive_timer.Enabled = true;
                        successive_time_elapsed = 0;
                        start_timestamp = GetCurrentTimestamp();
                    }

                    arm_Info.netState = ConnectionStatus.CONNECTED_KNOCK_DOOR_WAIT;

                    // this is an early stage detector which will validating if the training time user inputted is wrong,
                    // which means they may set 0 minutes to training time. note that this is not a good impl, we should
                    // move this part to settingup stage, which can throw error before start of the training. TODO
                    if (GetCurrentTimestamp() >= EndTimeStamp && !Freeway_mode.Checked && !Successive_mode.Checked)
                    {
                        TimeAccumulationTimer.Enabled = false;
                        TimeLeft.Text = "0 : 0";
                        arm_Info.netState = ConnectionStatus.END_TRAINING_IN_PROGRESS;
                        break;
                    }
                    break;
                case ConnectionStatus.CONNECTED_KNOCK_DOOR_WAIT:
                    if (!GlobalBuffer.g_dataNeedProcess)
                    {
                        timeoutCount++;
                        if (timeoutCount >= 3)
                        {
                            timeoutCount = 0;
                            arm_Info.netState = ConnectionStatus.CONNECTED_KNOCK_DOOR;
                        }
                    }
                    else
                    {
                        timeoutCount = 0; // for next use 
                        ConnectionState.Text = "Connected";
                        arm_Info.netState = ConnectionStatus.CONNECTED;
                        GlobalBuffer.g_dataNeedProcess = false;
                        receiveDataList.Clear();
                        SettingStopButton.Enabled = true;

                        if (Freeway_mode.Checked)
                            trainingTimeElapsed.Enabled = true;
                    }
                    break;
                case ConnectionStatus.CONNECTED:

                    // successive mode need this function, hence we just skip it in this statement just like normal mode
                    if (GetCurrentTimestamp() >= EndTimeStamp && !Freeway_mode.Checked)
                    {
                        TimeAccumulationTimer.Enabled = false;
                        TimeLeft.Text = "0 : 0";
                        arm_Info.netState = ConnectionStatus.END_TRAINING_IN_PROGRESS;
                        break;
                    }
                    ushort RatPos;
                    ushort Speed;
                    double RecordPos = 0;
                    TrainingState.Text = "In Progress";
                    if (!GlobalBuffer.g_dataNeedProcess)
                        break;

                    if (Successive_mode.Checked) 
                    {
                        /* location displaying for wheel 0 */
                        RatPos = receiveDataList[0];
                        switch (RatPos)
                        {
                            case 1:
                                RatLeftAngle.ForeColor = Color.Red;
                                RatPos = (ushort)AngleSettingLR.Value;
                                RatLeftAngle.Text = RatPos.ToString();
                                break;
                            case 2:
                                RatLeftAngle.ForeColor = Color.Yellow;
                                RatPos = (ushort)AngleSettingLY.Value;
                                RatLeftAngle.Text = RatPos.ToString();
                                break;
                            case 3:
                                RatLeftAngle.ForeColor = Color.Green;
                                RatPos = (ushort)AngleSettingLG.Value;
                                RatLeftAngle.Text = RatPos.ToString();
                                break;
                            case 4:
                                RatLeftAngle.ForeColor = Color.Blue;
                                RatPos = (ushort)AngleSettingLB.Value;
                                RatLeftAngle.Text = RatPos.ToString();
                                break;
                            case 5:
                                RatLeftAngle.ForeColor = Color.Purple;
                                RatPos = (ushort)AngleSettingLP.Value;
                                RatLeftAngle.Text = RatPos.ToString();
                                break;
                        }
                        resultStreamWriter.WriteLine("L: " + RatPos.ToString() + "°" + "  " + ui_speed[(((GetCurrentTimestamp() - start_timestamp) / successive_L_tv) + 1) % ui_speed.Count()] + " m/min");

                        /* ------------------------------- */
                        /* location displaying for wheel 1 */
                        RatPos = receiveDataList[1];
                        //---------------------ratPos representation----
                        switch (RatPos)
                        {
                            case 1:
                                RatMiddleAngle.ForeColor = Color.Red;
                                RatPos = (ushort)AngleSettingMR.Value;
                                RatMiddleAngle.Text = RatPos.ToString();
                                RatPos = 10;
                                break;
                            case 2:
                                RatMiddleAngle.ForeColor = Color.Yellow;
                                RatPos = (ushort)AngleSettingMY.Value;
                                RatMiddleAngle.Text = RatPos.ToString();
                                break;
                            case 3:
                                RatMiddleAngle.ForeColor = Color.Green;
                                RatPos = (ushort)AngleSettingMG.Value;
                                RatMiddleAngle.Text = RatPos.ToString();
                                break;
                            case 4:
                                RatMiddleAngle.ForeColor = Color.Blue;
                                RatPos = (ushort)AngleSettingMB.Value;
                                RatMiddleAngle.Text = RatPos.ToString();
                                break;
                            case 5:
                                RatMiddleAngle.ForeColor = Color.Purple;
                                RatPos = (ushort)AngleSettingMP.Value;
                                RatMiddleAngle.Text = RatPos.ToString();
                                break;
                        }
                        resultStreamWriter.WriteLine("M: " + RatPos.ToString() + "°" + "  " + ui_speed[(((GetCurrentTimestamp() - start_timestamp) / successive_M_tv) + 1) % ui_speed.Count()] + " m/min");

                        /* ------------------------------- */

                        /* location displaying for wheel 1 */
                        RatPos = receiveDataList[2];
                        switch (RatPos)
                        {
                            case 1:
                                RatRightAngle.ForeColor = Color.Red;
                                RatPos = (ushort)AngleSettingRR.Value;
                                RatRightAngle.Text = RatPos.ToString();
                                break;
                            case 2:
                                RatRightAngle.ForeColor = Color.Yellow;
                                RatPos = (ushort)AngleSettingRY.Value;
                                RatRightAngle.Text = RatPos.ToString();
                                break;
                            case 3:
                                RatRightAngle.ForeColor = Color.Green;
                                RatPos = (ushort)AngleSettingRG.Value;
                                RatRightAngle.Text = RatPos.ToString();
                                break;
                            case 4:
                                RatRightAngle.ForeColor = Color.Blue;
                                RatPos = (ushort)AngleSettingRB.Value;
                                RatRightAngle.Text = RatPos.ToString();
                                break;
                            case 5:
                                RatRightAngle.ForeColor = Color.Purple;
                                RatPos = (ushort)AngleSettingRP.Value;
                                RatRightAngle.Text = RatPos.ToString();
                                break;
                        }

                        // we get speed from current speed level. plus one is bacause level starts from one but our divsion at early start is 0
                        resultStreamWriter.WriteLine("R: " + RatPos.ToString() + "°" + "  " + ui_speed[(((GetCurrentTimestamp() - start_timestamp) / successive_R_tv) + 1) % ui_speed.Count()] + " m/min");
                        /* ------------------------------- */

                        serialPort.Write(DACspeed, 0, 6);
                        receiveDataList.Clear();
                        GlobalBuffer.g_dataNeedProcess = false;
                        break;
                    }
                    else if (Freeway_mode.Checked)
                    {
                        if (receiveDataList[0] >> 7 == 1)
                        {
                            round_L += 0.25;
                            L_roundCount.Text = round_L.ToString();
                            L_avgSpeed.Text = ((round_L * wheelDiameter) / Freeway_timerCount).ToString("#.##");
                        }
                        if (receiveDataList[1] >> 7 == 1)
                        {
                            round_M += 0.25;
                            M_roundCount.Text = round_M.ToString();
                            M_avgSpeed.Text = ((round_M * wheelDiameter) / Freeway_timerCount).ToString("#.##");
                        }
                        if (receiveDataList[2] >> 7 == 1)
                        {
                            round_R += 0.25;
                            R_roundCount.Text = round_R.ToString();
                            R_avgSpeed.Text = ((round_R * wheelDiameter) / Freeway_timerCount).ToString("#.##");
                        }
                        serialPort.Write(DACspeed, 0, 6);
                        receiveDataList.Clear();
                        GlobalBuffer.g_dataNeedProcess = false;
                        break;
                    }
                    for (ushort i = 0; i < 3; i++)//Ratseat class 規律設定
                    {
                        switch (i)
                        {
                            case 0:
                                switch (receiveDataList[i])//變數待商榷
                                {
                                    case 1:
                                        DACspeed[0] = (byte)DACtable[(ushort)SpeedSettingLR.Value];
                                        DACspeed[1] = (byte)(DACtable[(ushort)SpeedSettingLR.Value] >> 8);
                                        break;
                                    case 2:
                                        DACspeed[0] = (byte)DACtable[(ushort)SpeedSettingLY.Value];
                                        DACspeed[1] = (byte)(DACtable[(ushort)SpeedSettingLY.Value] >> 8);
                                        break;
                                    case 3:
                                        DACspeed[0] = (byte)DACtable[(ushort)SpeedSettingLG.Value];
                                        DACspeed[1] = (byte)(DACtable[(ushort)SpeedSettingLG.Value] >> 8);
                                        break;
                                    case 4:
                                        DACspeed[0] = (byte)DACtable[(ushort)SpeedSettingLB.Value];
                                        DACspeed[1] = (byte)(DACtable[(ushort)SpeedSettingLB.Value] >> 8);
                                        break;
                                    case 5:
                                        DACspeed[0] = (byte)DACtable[(ushort)SpeedSettingLP.Value];
                                        DACspeed[1] = (byte)(DACtable[(ushort)SpeedSettingLP.Value] >> 8);
                                        break;
                                }
                                break;
                            case 1:
                                switch (receiveDataList[i])
                                {
                                    case 1:
                                        DACspeed[2] = (byte)DACtable[(ushort)SpeedSettingMR.Value];
                                        DACspeed[3] = (byte)(DACtable[(ushort)SpeedSettingMR.Value] >> 8);
                                        break;
                                    case 2:
                                        DACspeed[2] = (byte)DACtable[(ushort)SpeedSettingMY.Value];
                                        DACspeed[3] = (byte)(DACtable[(ushort)SpeedSettingMY.Value] >> 8);
                                        break;
                                    case 3:
                                        DACspeed[2] = (byte)DACtable[(ushort)SpeedSettingMG.Value];
                                        DACspeed[3] = (byte)(DACtable[(ushort)SpeedSettingMG.Value] >> 8);
                                        break;
                                    case 4:
                                        DACspeed[2] = (byte)DACtable[(ushort)SpeedSettingMB.Value];
                                        DACspeed[3] = (byte)(DACtable[(ushort)SpeedSettingMB.Value] >> 8);
                                        break;
                                    case 5:
                                        DACspeed[2] = (byte)DACtable[(ushort)SpeedSettingMP.Value];
                                        DACspeed[3] = (byte)(DACtable[(ushort)SpeedSettingMP.Value] >> 8);
                                        break;
                                }
                                break;
                            case 2:
                                switch (receiveDataList[i])
                                {
                                    case 1:
                                        DACspeed[4] = (byte)DACtable[(ushort)SpeedSettingRR.Value];
                                        DACspeed[5] = (byte)(DACtable[(ushort)SpeedSettingRR.Value] >> 8);
                                        break;
                                    case 2:
                                        DACspeed[4] = (byte)DACtable[(ushort)SpeedSettingRY.Value];
                                        DACspeed[5] = (byte)(DACtable[(ushort)SpeedSettingRY.Value] >> 8);
                                        break;
                                    case 3:
                                        DACspeed[4] = (byte)DACtable[(ushort)SpeedSettingRG.Value];
                                        DACspeed[5] = (byte)(DACtable[(ushort)SpeedSettingRG.Value] >> 8);
                                        break;
                                    case 4:
                                        DACspeed[4] = (byte)DACtable[(ushort)SpeedSettingRB.Value];
                                        DACspeed[5] = (byte)(DACtable[(ushort)SpeedSettingRB.Value] >> 8);
                                        break;
                                    case 5:
                                        DACspeed[4] = (byte)DACtable[(ushort)SpeedSettingRP.Value];
                                        DACspeed[5] = (byte)(DACtable[(ushort)SpeedSettingRP.Value] >> 8);
                                        break;
                                }
                                break;
                        }
                    }
                    //----------Data Record-----------
                    RatPos = receiveDataList[0];
                    switch (RatPos)
                    {
                        case 1:
                            RatLeftAngle.ForeColor = Color.Red;
                            RatPos = (ushort)AngleSettingLR.Value;
                            RatLeftAngle.Text = RatPos.ToString();
                            break;
                        case 2:
                            RatLeftAngle.ForeColor = Color.Yellow;
                            RatPos = (ushort)AngleSettingLY.Value;
                            RatLeftAngle.Text = RatPos.ToString();
                            break;
                        case 3:
                            RatLeftAngle.ForeColor = Color.Green;
                            RatPos = (ushort)AngleSettingLG.Value;
                            RatLeftAngle.Text = RatPos.ToString();
                            break;
                        case 4:
                            RatLeftAngle.ForeColor = Color.Blue;
                            RatPos = (ushort)AngleSettingLB.Value;
                            RatLeftAngle.Text = RatPos.ToString();
                            break;
                        case 5:
                            RatLeftAngle.ForeColor = Color.Purple;
                            RatPos = (ushort)AngleSettingLP.Value;
                            RatLeftAngle.Text = RatPos.ToString();
                            break;
                    }
                    //----------------------------------------------
                    Speed = (ushort)(DACspeed[1] << 8);
                    Speed |= DACspeed[0];
                    switch (Speed) //  0x0, 0x020d, 0x041a, 0x0628
                    {
                        case 0x0000:
                            RecordPos = 0; 
                            RatLeftSpeed.Text = "0";
                            break;
                        case 0x0174:
                            RecordPos = 1.13;
                            RatLeftSpeed.Text = "1.13";
                            break;
                        case 0x02E8:
                            RecordPos = 1.63;
                            RatLeftSpeed.Text = "1.63";
                            break;
                        case 0x045D:
                            RecordPos = 3.01;
                            RatLeftSpeed.Text = "3.01";
                            break;
                        case 0x5D1:
                            RecordPos = 3.36;
                            RatLeftSpeed.Text = "3.36";
                            break;
                        case 0x0745:
                            RecordPos = 4.51;
                            RatLeftSpeed.Text = "4.51";
                            break;
                        case 0x08B9:
                            RecordPos = 5.05;
                            RatLeftSpeed.Text = "5.05";
                            break;
                        case 0x0A2D:
                            RecordPos = 6.27;
                            RatLeftSpeed.Text = "6.27";
                            break;
                        case 0x0BA2:
                            RecordPos = 6.77;
                            RatLeftSpeed.Text = "6.77";
                            break;
                        case 0x0D16:
                            RecordPos = 8.04;
                            RatLeftSpeed.Text = "8.04";
                            break;
                        case 0x0E8A:
                            RecordPos = 8.67;
                            RatLeftSpeed.Text = "8.67";
                            break;
                    // POS case 3 deprecated since we changed total POS from 4 to 3 
                    }
                    resultStreamWriter.WriteLine("L: " + RatPos.ToString() + "°" + "  " + RecordPos.ToString() + " m/min");
                    RatPos = receiveDataList[1];
                    //---------------------ratPos representation----
                    switch (RatPos)
                    {
                        case 1:
                            RatMiddleAngle.ForeColor = Color.Red;
                            RatPos = (ushort)AngleSettingMR.Value;
                            RatMiddleAngle.Text = RatPos.ToString();
                            RatPos = 10;
                            break;
                        case 2:
                            RatMiddleAngle.ForeColor = Color.Yellow;
                            RatPos = (ushort)AngleSettingMY.Value;
                            RatMiddleAngle.Text = RatPos.ToString();
                            break;
                        case 3:
                            RatMiddleAngle.ForeColor = Color.Green;
                            RatPos = (ushort)AngleSettingMG.Value;
                            RatMiddleAngle.Text = RatPos.ToString();
                            break;
                        case 4:
                            RatMiddleAngle.ForeColor = Color.Blue;
                            RatPos = (ushort)AngleSettingMB.Value;
                            RatMiddleAngle.Text = RatPos.ToString();
                            break;
                        case 5:
                            RatMiddleAngle.ForeColor = Color.Purple;
                            RatPos = (ushort)AngleSettingMP.Value;
                            RatMiddleAngle.Text = RatPos.ToString();
                            break;
                    }
                    //----------------------------------------------
                    Speed = (ushort)(DACspeed[3] << 8);
                    Speed |= DACspeed[2];
                    switch (Speed) //  0x0, 0x020d, 0x041a, 0x0628
                    {
                        case 0x0000:
                            RecordPos = 0;
                            RatMiddleSpeed.Text = "0";
                            break;
                        case 0x0174:
                            RecordPos = 1.13;
                            RatMiddleSpeed.Text = "1.13";
                            break;
                        case 0x02E8:
                            RecordPos = 1.63;
                            RatMiddleSpeed.Text = "1.63";
                            break;
                        case 0x045D:
                            RecordPos = 3.01;
                            RatMiddleSpeed.Text = "3.01";
                            break;
                        case 0x5D1:
                            RecordPos = 3.36;
                            RatMiddleSpeed.Text = "3.36";
                            break;
                        case 0x0745:
                            RecordPos = 4.51;
                            RatMiddleSpeed.Text = "4.51";
                            break;
                        case 0x08B9:
                            RecordPos = 5.05;
                            RatMiddleSpeed.Text = "5.05";
                            break;
                        case 0x0A2D:
                            RecordPos = 6.27;
                            RatMiddleSpeed.Text = "6.27";
                            break;
                        case 0x0BA2:
                            RecordPos = 6.77;
                            RatMiddleSpeed.Text = "6.77";
                            break;
                        case 0x0D16:
                            RecordPos = 8.04;
                            RatMiddleSpeed.Text = "8.04";
                            break;
                        case 0x0E8A:
                            RecordPos = 8.67;
                            RatMiddleSpeed.Text = "8.67";
                            break;
                    }
                    resultStreamWriter.WriteLine("M: " + RatPos.ToString() + "°" + "  " + RecordPos.ToString() + " m/min");
                    RatPos = receiveDataList[2]; 
                    switch (RatPos)
                    {
                        case 1:
                            RatRightAngle.ForeColor = Color.Red;
                            RatPos = (ushort)AngleSettingRR.Value;
                            RatRightAngle.Text = RatPos.ToString();
                            break;
                        case 2:
                            RatRightAngle.ForeColor = Color.Yellow;
                            RatPos = (ushort)AngleSettingRY.Value;
                            RatRightAngle.Text = RatPos.ToString();
                            break;
                        case 3:
                            RatRightAngle.ForeColor = Color.Green;
                            RatPos = (ushort)AngleSettingRG.Value;
                            RatRightAngle.Text = RatPos.ToString();
                            break;
                        case 4:
                            RatRightAngle.ForeColor = Color.Blue;
                            RatPos = (ushort)AngleSettingRB.Value;
                            RatRightAngle.Text = RatPos.ToString();
                            break;
                        case 5:
                            RatRightAngle.ForeColor = Color.Purple;
                            RatPos = (ushort)AngleSettingRP.Value;
                            RatRightAngle.Text = RatPos.ToString();
                            break;
                    }
                    //----------------------------------------------
                    Speed = (ushort)(DACspeed[5] << 8);
                    Speed |= DACspeed[4];
                    switch (Speed) //  0x0, 0x020d, 0x041a, 0x0628
                    {
                        case 0x0000:
                            RecordPos = 0;
                            RatRightSpeed.Text = "0";
                            break;
                        case 0x0174:
                            RecordPos = 1.13;
                            RatRightSpeed.Text = "1.13";
                            break;
                        case 0x02E8:
                            RecordPos = 1.63;
                            RatRightSpeed.Text = "1.63";
                            break;
                        case 0x045D:
                            RecordPos = 3.01;
                            RatRightSpeed.Text = "3.01";
                            break;
                        case 0x5D1:
                            RecordPos = 3.36;
                            RatRightSpeed.Text = "3.36";
                            break;
                        case 0x0745:
                            RecordPos = 4.51;
                            RatRightSpeed.Text = "4.51";
                            break;
                        case 0x08B9:
                            RecordPos = 5.05;
                            RatRightSpeed.Text = "5.05";
                            break;
                        case 0x0A2D:
                            RecordPos = 6.27;
                            RatRightSpeed.Text = "6.27";
                            break;
                        case 0x0BA2:
                            RecordPos = 6.77;
                            RatRightSpeed.Text = "6.77";
                            break;
                        case 0x0D16:
                            RecordPos = 8.04;
                            RatRightSpeed.Text = "8.04";
                            break;
                        case 0x0E8A:
                            RecordPos = 8.67;
                            RatRightSpeed.Text = "8.67";
                            break;
                    }
                    resultStreamWriter.WriteLine("R: " + RatPos.ToString() + "°" + "  " + RecordPos.ToString() + " m/min" + "\r\n");

                    resultStreamWriter.Flush();
                    serialPort.Write(DACspeed, 0, 6); // we don't clean DACspeed is because that each time we write whole size of it with new data 
                    receiveDataList.Clear();
                    GlobalBuffer.g_dataNeedProcess = false;
                    break;
                case ConnectionStatus.END_TRAINING_IN_PROGRESS:
                    serialPort.Write(new char[6] { '~', 'N', 'D', 'O', 'F', 'T' }, 0, 6); 
                    arm_Info.netState = ConnectionStatus.END_TRAINING_WAIT_PROGRESS;
                    break;
                case ConnectionStatus.END_TRAINING_WAIT_PROGRESS:
                    if (!GlobalBuffer.g_dataNeedProcess)
                    {
                        timeoutCount++;
                        if (timeoutCount >= 3)
                        {
                            timeoutCount = 0;
                            arm_Info.netState = ConnectionStatus.END_TRAINING_IN_PROGRESS;
                        }
                    }
                    else
                    {
                        timeoutCount = 0;
                        if (receiveDataList[0] != 'A' || receiveDataList[1] != 'C')
                        {
                            arm_Info.netState = ConnectionStatus.END_TRAINING_IN_PROGRESS;
                            break;
                        }
                        arm_Info.netState = ConnectionStatus.TRAINING_END; // training end state switched here 
                    }
                    break;
                case ConnectionStatus.TRAINING_END:
                    Successive_timer.Enabled = false;
                    if (Freeway_mode.Checked)
                    {
                        resultStreamWriter.WriteLine("Left wheel avg. speed: " + ((round_L * wheelDiameter) / Freeway_timerCount).ToString() + " m/min" + "\r\n");
                        resultStreamWriter.WriteLine("Middle wheel avg. speed: " + ((round_M * wheelDiameter) / Freeway_timerCount).ToString() + " m/min" + "\r\n");
                        resultStreamWriter.WriteLine("Right wheel avg. speed: " + ((round_R * wheelDiameter) / Freeway_timerCount).ToString() + " m/min" + "\r\n");
                        resultStreamWriter.WriteLine("Total training time: " + (Freeway_timerCount / 60).ToString() + "minute(s) " + (Freeway_timerCount % 60).ToString() + "second(s)" +"\r\n");

                        resultStreamWriter.Flush();

                        Freeway_timerCount = 0;
                        round_L = 0;
                        round_M = 0;
                        round_R = 0;
                    }

                    /* insertion of rat ID (log file) */
                    if (L_rat_id.Text != "") resultStreamWriter.WriteLine("Rat ID (Left): " + L_rat_id.Text);
                    if (M_rat_id.Text != "") resultStreamWriter.WriteLine("Rat ID (Middle): " + M_rat_id.Text);
                    if (R_rat_id.Text != "") resultStreamWriter.WriteLine("Rat ID (Right): " + R_rat_id.Text + "\r\n");


                    resultStreamWriter.Write("---------End of training---------" + "\n\r\n");
                    RatLeftAngle.BackColor = Color.DarkGray;
                    RatLeftAngle.BackColor = Color.DarkGray;
                    RatMiddleAngle.BackColor = Color.DarkGray;
                    RatMiddleAngle.BackColor = Color.DarkGray;
                    RatRightAngle.BackColor = Color.DarkGray;
                    RatRightAngle.BackColor = Color.DarkGray;
                    serialPort.Close();
                    resultStreamWriter.Close();
                    TrainingState.Text = "Training end";
                    SettingStartButton.BackColor = Color.LawnGreen;
                    Console.Beep(1000, 1500);
                    NetworkTimer.Enabled = false;
                    SettingStopButton.Enabled = false;
                    GlobalBuffer.g_dataNeedProcess = false;
                    RatLeftAngle.Text = "";
                    RatMiddleAngle.Text = "";
                    RatRightAngle.Text = "";
                    RatLeftSpeed.Text = "";
                    RatMiddleSpeed.Text = "";
                    RatRightSpeed.Text = "";
                    break;

            }
        }
        private void OnSerialPortReceive(Object sender, SerialDataReceivedEventArgs e)
        {
            try
            {
                Byte[] buffer = new Byte[3]; // WARN: once checksum added, the buffer size should increase 
                int length = (sender as SerialPort).Read(buffer, 0, buffer.Length);
                for (int i = 0; i < length; i++)
                {
                    receiveDataList.Add(buffer[i]);
                }
                GlobalBuffer.g_dataNeedProcess = true;
                debug_t++;
            }
            catch
            {
                serialPort.Close();
            }
        }

        private void ResultFilePath_TextChanged(object sender, EventArgs e)//檔案類 顯示組
        {
            
        }   
        private void FileSelectButton_Click(object sender, EventArgs e)//檔案類 按鍵組
        {
            ResultFileDialog.ShowDialog();
        }

        private void Preset_Click(object sender, EventArgs e)//設定類 按鍵組 預設03531 12421 14640 30 60 90 120 150
        {
            //滾輪類 左側 速度組
            SpeedSettingLR.Value = 0;
            SpeedSettingLY.Value = 3;
            SpeedSettingLG.Value = 5;
            SpeedSettingLB.Value = 3; 
            SpeedSettingLP.Value = 1;
            //滾輪類 中間 速度組
            SpeedSettingMR.Value = 1;
            SpeedSettingMY.Value = 2;
            SpeedSettingMG.Value = 4;
            SpeedSettingMB.Value = 2;
            SpeedSettingMP.Value = 1;
            //滾輪類 右側 速度組
            SpeedSettingRR.Value = 1;
            SpeedSettingRY.Value = 4;
            SpeedSettingRG.Value = 6;
            SpeedSettingRB.Value = 4;
            SpeedSettingRP.Value = 0;

            //滾輪類 左側 角度組
            AngleSettingLR.Value = 30;
            AngleSettingLY.Value = 60;
            AngleSettingLG.Value = 90;
            AngleSettingLB.Value = 120;
            AngleSettingLP.Value = 150;
            label5_17.Text = AngleSettingLR.Value.ToString() + "°";
            label5_27.Text = AngleSettingLY.Value.ToString() + "°";
            label5_37.Text = AngleSettingLG.Value.ToString() + "°";
            label5_47.Text = AngleSettingLB.Value.ToString() + "°";
            label5_57.Text = AngleSettingLP.Value.ToString() + "°";
            //滾輪類 中間 角度組
            AngleSettingMR.Value = 30;
            AngleSettingMY.Value = 60;
            AngleSettingMG.Value = 90;
            AngleSettingMB.Value = 120;
            AngleSettingMP.Value = 150;
            label6_17.Text = AngleSettingMR.Value.ToString() + "°";
            label6_27.Text = AngleSettingMY.Value.ToString() + "°";
            label6_37.Text = AngleSettingMG.Value.ToString() + "°";
            label6_47.Text = AngleSettingMB.Value.ToString() + "°";
            label6_57.Text = AngleSettingMP.Value.ToString() + "°";
            //滾輪類 右側 角度組
            AngleSettingRR.Value = 30;
            AngleSettingRY.Value = 60;
            AngleSettingRG.Value = 90;
            AngleSettingRB.Value = 120;
            AngleSettingRP.Value = 150;
            label7_17.Text = AngleSettingRR.Value.ToString() + "°";
            label7_27.Text = AngleSettingRY.Value.ToString() + "°";
            label7_37.Text = AngleSettingRG.Value.ToString() + "°";
            label7_47.Text = AngleSettingRB.Value.ToString() + "°";
            label7_57.Text = AngleSettingRP.Value.ToString() + "°";

            //SerialPortSelect.Text = "COM1";//連線位址 deprecated since it almost never got right prediction
            TimeConsuming.Value = 10;//訓練時間10分鐘
            
        }

        private void SetUp_Click(object sender, EventArgs e)//設定類 按鍵組
        {
            Data();            
            if (Freeway_mode.Checked || Successive_mode.Checked) // if Freeway_mode enabled, we set the schdule bar to 100 directly
                Schedule.Value = 100;
            if (Schedule.Value < 99)
            {
                Schedule.Value = 100;
                /* ignore sensor angle check
                //檢查固定的AngleSettingLG、AngleSettingMG、AngleSettingRG是否為90°
                int D = AngleSettingLG.Value, E = AngleSettingMG.Value, F = AngleSettingRG.Value;
                if (D == 90 && E == 90 && F == 90)
                {
                    AngleSettingLG.BackColor = Color.WhiteSmoke;
                    AngleSettingMG.BackColor = Color.WhiteSmoke;
                    AngleSettingRG.BackColor = Color.WhiteSmoke;
                    Schedule.Value += 10;

                }
                else
                {
                    AngleSettingLG.BackColor = Color.Red;
                    AngleSettingMG.BackColor = Color.Red;
                    AngleSettingRG.BackColor = Color.Red;
                }

                //檢查AngleSettingLR、AngleSettingLY、AngleSettingLB、AngleSettingLP，大小是否為1<2<3<4<5
                int G = AngleSettingLR.Value, H = AngleSettingLY.Value, I = AngleSettingLB.Value, J = AngleSettingLP.Value;
                if (G < H && I < J)
                {
                    AngleSettingLR.BackColor = Color.WhiteSmoke;
                    AngleSettingLY.BackColor = Color.WhiteSmoke;
                    AngleSettingLB.BackColor = Color.WhiteSmoke;
                    AngleSettingLP.BackColor = Color.WhiteSmoke;
                    Schedule.Value += 10;
                }
                else if (G > H && I < J)
                {
                    AngleSettingLR.BackColor = Color.Red;
                    AngleSettingLY.BackColor = Color.Red;
                    AngleSettingLB.BackColor = Color.WhiteSmoke;
                    AngleSettingLP.BackColor = Color.WhiteSmoke;
                    MessageBox.Show("左側滾輪機台的第一及第二，感測器角度設定1>2，請檢察，謝謝! ");
                }
                else if (G < H && I > J)
                {
                    AngleSettingLR.BackColor = Color.WhiteSmoke;
                    AngleSettingLY.BackColor = Color.WhiteSmoke;
                    AngleSettingLB.BackColor = Color.Red;
                    AngleSettingLP.BackColor = Color.Red;
                    MessageBox.Show("左側滾輪機台的第四及第五，感測器角度設定4>5，請檢察，謝謝! ");
                }
                else
                {
                    AngleSettingLR.BackColor = Color.Red;
                    AngleSettingLY.BackColor = Color.Red;
                    AngleSettingLB.BackColor = Color.Red;
                    AngleSettingLP.BackColor = Color.Red;
                    MessageBox.Show("左側滾輪機台的第一及第二與第四及第五，感測器角度設定1>2、4>5，請檢察，謝謝!! ");
                }

                //檢查AngleSettingMR、AngleSettingMY、AngleSettingMB、AngleSettingMP，大小是否為1<2<3<4<5
                G = AngleSettingMR.Value; H = AngleSettingMY.Value; I = AngleSettingMB.Value; J = AngleSettingMP.Value;
                if (G < H && I < J)
                {
                    AngleSettingMR.BackColor = Color.WhiteSmoke;
                    AngleSettingMY.BackColor = Color.WhiteSmoke;
                    AngleSettingMB.BackColor = Color.WhiteSmoke;
                    AngleSettingMP.BackColor = Color.WhiteSmoke;
                    Schedule.Value += 10;
                }
                else if (G > H && I < J)
                {
                    AngleSettingMR.BackColor = Color.Red;
                    AngleSettingMY.BackColor = Color.Red;
                    AngleSettingMB.BackColor = Color.WhiteSmoke;
                    AngleSettingMP.BackColor = Color.WhiteSmoke;
                    MessageBox.Show("中間滾輪機台的第一及第二，感測器角度設定1>2，請檢察，謝謝! ");
                }
                else if (G < H && I > J)
                {
                    AngleSettingMR.BackColor = Color.WhiteSmoke;
                    AngleSettingMY.BackColor = Color.WhiteSmoke;
                    AngleSettingMB.BackColor = Color.Red;
                    AngleSettingMP.BackColor = Color.Red;
                    MessageBox.Show("中間滾輪機台的第四及第五，感測器角度設定4>5，請檢察，謝謝! ");
                }
                else
                {
                    AngleSettingMR.BackColor = Color.Red;
                    AngleSettingMY.BackColor = Color.Red;
                    AngleSettingMB.BackColor = Color.Red;
                    AngleSettingMP.BackColor = Color.Red;
                    MessageBox.Show("中間滾輪機台的第一及第二與第四及第五，感測器角度設定1>2、4>5，請檢察，謝謝!! ");
                }

                //檢查AngleSettingRR、AngleSettingRY、AngleSettingRB、AngleSettingRP，大小是否為1<2<3<4<5
                G = AngleSettingRR.Value; H = AngleSettingRY.Value; I = AngleSettingRB.Value; J = AngleSettingRP.Value;
                if (G < H && I < J)
                {
                    AngleSettingRR.BackColor = Color.WhiteSmoke;
                    AngleSettingRY.BackColor = Color.WhiteSmoke;
                    AngleSettingRB.BackColor = Color.WhiteSmoke;
                    AngleSettingRP.BackColor = Color.WhiteSmoke;
                    Schedule.Value += 10;
                }
                else if (G > H && I < J)
                {
                    AngleSettingRR.BackColor = Color.Red;
                    AngleSettingRY.BackColor = Color.Red;
                    AngleSettingRB.BackColor = Color.WhiteSmoke;
                    AngleSettingRP.BackColor = Color.WhiteSmoke;
                    MessageBox.Show("右側滾輪機台的第一及第二，感測器角度設定1>2，請檢察，謝謝! ");
                }
                else if (G < H && I > J)
                {
                    AngleSettingRR.BackColor = Color.WhiteSmoke;
                    AngleSettingRY.BackColor = Color.WhiteSmoke;
                    AngleSettingRB.BackColor = Color.Red;
                    AngleSettingRP.BackColor = Color.Red;
                    MessageBox.Show("右側滾輪機台的第四及第五，感測器角度設定4>5，請檢察，謝謝! ");
                }
                else
                {
                    AngleSettingRR.BackColor = Color.Red;
                    AngleSettingRY.BackColor = Color.Red;
                    AngleSettingRB.BackColor = Color.Red;
                    AngleSettingRP.BackColor = Color.Red;
                    MessageBox.Show("右側滾輪機台的第一及第二與第四及第五，感測器角度設定1>2、4>5，請檢察，謝謝!! ");
                }
                */
                //檢查SpeedSettingLR、SpeedSettingLY、SpeedSettingLG、SpeedSettingLB、SpeedSettingLP有沒有設定
                int K = (int)SpeedSettingLR.Value, L = (int)SpeedSettingLY.Value, M = (int)SpeedSettingLG.Value, N = (int)SpeedSettingLB.Value, O = (int)SpeedSettingLP.Value;
                if (K == 0 && L == 0 && M == 0 && N == 0 && O == 0)
                {
                    SpeedSettingLR.BackColor = Color.Red;
                    SpeedSettingLY.BackColor = Color.Red;
                    SpeedSettingLG.BackColor = Color.Red;
                    SpeedSettingLB.BackColor = Color.Red;
                    SpeedSettingLP.BackColor = Color.Red;
                    // MessageBox.Show("左側滾輪的速度參數請選擇，謝謝!!");
                }
                else if (L != 0 && M != 0 && N != 0)
                {
                    SpeedSettingLR.BackColor = Color.White;
                    SpeedSettingLY.BackColor = Color.White;
                    SpeedSettingLG.BackColor = Color.White;
                    SpeedSettingLB.BackColor = Color.White;
                    SpeedSettingLP.BackColor = Color.White;
                    Schedule.Value += 10;
                    // MessageBox.Show("左側滾輪，第一及第五的速度參數設定是否有，為0的存在!");
                }

                //檢查SpeedSettingMR、SpeedSettingMY、SpeedSettingMG、SpeedSettingMB、SpeedSettingMP有沒有設定
                K = (int)SpeedSettingMR.Value; L = (int)SpeedSettingMY.Value; M = (int)SpeedSettingMG.Value; N = (int)SpeedSettingMB.Value; O = (int)SpeedSettingMP.Value;
                if (K == 0 && L == 0 && M == 0 && N == 0 && O == 0)
                {
                    SpeedSettingMR.BackColor = Color.Red;
                    SpeedSettingMY.BackColor = Color.Red;
                    SpeedSettingMG.BackColor = Color.Red;
                    SpeedSettingMB.BackColor = Color.Red;
                    SpeedSettingMP.BackColor = Color.Red;
                   //  MessageBox.Show("中間滾輪的速度參數請選擇，謝謝!!");
                }
                else if (L != 0 && M != 0 && N != 0)
                {
                    SpeedSettingMR.BackColor = Color.White;
                    SpeedSettingMY.BackColor = Color.White;
                    SpeedSettingMG.BackColor = Color.White;
                    SpeedSettingMB.BackColor = Color.White;
                    SpeedSettingMP.BackColor = Color.White;
                    Schedule.Value += 10;
                    // MessageBox.Show("中間滾輪，第一及第五的速度參數設定是否有，為0的存在!");
                }

                //檢查SpeedSettingRR、SpeedSettingRY、SpeedSettingRG、SpeedSettingRB、SpeedSettingRP有沒有設定
                K = (int)SpeedSettingRR.Value; L = (int)SpeedSettingRY.Value; M = (int)SpeedSettingRG.Value; N = (int)SpeedSettingRB.Value; O = (int)SpeedSettingRP.Value;
                if (K == 0 && L == 0 && M == 0 && N == 0 && O == 0)
                {
                    SpeedSettingRR.BackColor = Color.Red;
                    SpeedSettingRY.BackColor = Color.Red;
                    SpeedSettingRG.BackColor = Color.Red;
                    SpeedSettingRB.BackColor = Color.Red;
                    SpeedSettingRP.BackColor = Color.Red;
                    // MessageBox.Show("右側滾輪的速度參數請選擇，謝謝!!");
                }
                else if (L != 0 && M != 0 && N != 0)
                {
                    SpeedSettingRR.BackColor = Color.White;
                    SpeedSettingRY.BackColor = Color.White;
                    SpeedSettingRG.BackColor = Color.White;
                    SpeedSettingRB.BackColor = Color.White;
                    SpeedSettingRP.BackColor = Color.White;
                    Schedule.Value += 10;
                    // MessageBox.Show("");
                }

                // 時間設定最少1
                int A = (int)TimeConsuming.Value;
                if (A < 1)
                {
                    TimeConsuming.BackColor = Color.Red;
                }
                else
                {
                    TimeLeft.Text = TimeConsuming.Value.ToString()+"：00";
                    TimeConsuming.BackColor = Color.White;
                    Schedule.Value += 10;
                }

                //資料存放位置設定
                string B = ResultFilePath.Text;
                if (B == "")
                {
                    ResultFilePath.BackColor = Color.Red;
                }
                else
                {
                    ResultFilePath.BackColor = Color.White;
                    Schedule.Value += 10;
                }

                //Port 連線選擇
                string C = SerialPortSelect.Text;
                if (C == "" || C == "Port error" || C == "File path error")
                {
                    SerialPortSelect.ForeColor = Color.Red;
                }
                else
                {
                    SerialPortSelect.ForeColor = Color.Black;
                    Schedule.Value += 10;
                }
            }
            if (Schedule.Value >= 100)
            {      
                SettingStartButton.Enabled = true;
            }
        }

        private void SettingStartButton_Click_1(object sender, EventArgs e)//設定類 按鍵組
        {
            TrainingState.ForeColor = Color.Black;
            TrainingState.Text = "Training";

            try
            {
                SerialPortSelect.ForeColor = SystemColors.WindowText;
                serialPort.PortName = SerialPortSelect.Text;
                serialPort.DataBits = 8;
                serialPort.Parity = Parity.None;
                serialPort.StopBits = StopBits.One;
                serialPort.BaudRate = 115200;
            }
            catch
            {
                serialPort.Close();
                SerialPortSelect.ForeColor = Color.Red;
                SerialPortSelect.Text = "Port error";
                return;
            }
            try
            {
                SerialPortSelect.ForeColor = SystemColors.WindowText;
                serialPort.Open();
                serialPort.DiscardOutBuffer();
                serialPort.DiscardInBuffer();
                serialPort.DataReceived += new SerialDataReceivedEventHandler(OnSerialPortReceive);
                SerialPortSelect.ForeColor = Color.Black;
            }
            catch
            {
                serialPort.Close();
                SerialPortSelect.ForeColor = Color.Red;
                SerialPortSelect.Text = "Port error";
                ConnectionState.Text = "No proper port";
                return;
            }
            try
            {
                resultStreamWriter = new StreamWriter(ResultFileDialog.FileName, true);
            }
            catch
            {
                serialPort.Close();
                ResultFilePath.Text = "File path error";
                ResultFilePath.ForeColor = Color.Red;
                return;
            }

            SettingStopButton.Enabled = true; 
            EndTimeStamp = GetCurrentTimestamp() + long.Parse(TimeConsuming.Text) * 60;
            ResultFilePath.ForeColor = Color.Black;
            ResultFilePath.Text = ResultFileDialog.FileName;
            SettingStartButton.BackColor = Color.Orange;
            if (!Freeway_mode.Checked)
                TimeAccumulationTimer.Enabled = true;
            L_avgSpeed.Text = "";
            M_avgSpeed.Text = "";
            R_avgSpeed.Text = "";
            L_roundCount.Text = "";
            M_roundCount.Text = "";
            R_roundCount.Text = "";
            arm_Info.netState = ConnectionStatus.CONNECTED_KNOCK_DOOR;
            NetworkTimer.Enabled = true;
            return;
        }

        private void SettingStopButton_Click(object sender, EventArgs e)//設定類 按鍵組
        {
            TimeAccumulationTimer.Enabled = false;
            TimeLeft.Text = "0 : 0";
            arm_Info.netState = ConnectionStatus.END_TRAINING_IN_PROGRESS;
            SettingStartButton.BackColor = Color.White;
            TrainingState.ForeColor = Color.Red;
            TrainingState.Text = "Non-Training";
            trainingTimeElapsed.Enabled = false;            
        }

        private void L_avgSpeed_TextChanged(object sender, EventArgs e)
        {

        }

        private void trainingTimeElapsed_Tick(object sender, EventArgs e)
        {
            Freeway_trainingElapsedTime.Text = (Freeway_timerCount / 60).ToString() + ":" + (Freeway_timerCount % 60).ToString();
            Freeway_timerCount++;
        }

        private void CountdownTimer_Tick(object sender, EventArgs e)
        {

        }

        private void Successive_timer_Tick(object sender, EventArgs e)
        {
            if (successive_time_elapsed == 0)
            {
                // 2 will used on first update
                idx_L = 2;
                idx_M = 2;
                idx_R = 2;

                RatLeftSpeed.Text = "1.13";
                RatMiddleSpeed.Text = "1.13";
                RatRightSpeed.Text = "1.13";

            }

            successive_time_elapsed++;

            if (successive_time_elapsed % successive_L_tv == 0)
            {
                RatLeftSpeed.Text = ui_speed[idx_L++ % ui_speed.Count()]; // 11 is size of ui_speed currently
            }

            if (successive_time_elapsed % successive_M_tv == 0)
            {
                RatMiddleSpeed.Text = ui_speed[idx_M++ % ui_speed.Count()];
            }

            if (successive_time_elapsed % successive_R_tv == 0)
            {
                RatRightSpeed.Text = ui_speed[idx_R++ % ui_speed.Count()];
            }

        }
    }
}
