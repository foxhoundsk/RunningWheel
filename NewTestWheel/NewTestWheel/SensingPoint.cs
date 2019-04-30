using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace NewTestWheel
{
    class SensingPoint
    {
        //Property 屬性
        public int SensingID;//編號
        public string Name;//名稱
        public int Speed;//速度值
        public int Angle;//角度值
        public int Frequency;//被感應次數

        //Method 行為/功能
        public SensingPoint(int sensingID,string name,int speed,int angle)
        {
            SensingID = sensingID;
            Name = name;
            Speed = speed;
            Angle = angle;
            Frequency = 0;
        }
        public SensingPoint()
        {

        }
        /*
        ushort RatPos;
        ushort Speed;
        float RecordPos = 0;
        public string angle(ushort angle)
        {
            switch (RatPos)
                    {
                        case 1:
                            RatLeft1.BackColor = Color.FromArgb(255, 255, 0, 0);//(Alpha,Red,Green,Blue)
                            RatPos = 10;
                            break;
                        case 2:
                            RatLeft1.BackColor = Color.FromArgb(255, 0, 255, 0);//(Alpha,Red,Green,Blue)
                            RatPos = 40;
                            break;
                        case 3:
                            RatLeft1.BackColor = Color.FromArgb(255, 0, 0, 255);//(Alpha,Red,Green,Blue)
                            RatPos = 70;
                            break;
                        case 4:
                            RatLeft1.BackColor = Color.FromArgb(255, 0, 0, 255);//(Alpha,Red,Green,Blue)
                            RatPos = 70;
                            break;
                        case 5:
                            RatLeft1.BackColor = Color.FromArgb(255, 0, 0, 255);//(Alpha,Red,Green,Blue)
                            RatPos = 70;
                            break;
                    }
        }

        public string speed(ushort speed)
        {
            switch (Speed) //  0x0, 0x020d, 0x041a, 0x0628
                    {
                        case 0x0000:
                            RecordPos = 0; 
                            RatLeftSpeed.Text = "0";
                            break;
                        case 0x00F8:
                            RecordPos = 1;
                            RatLeftSpeed.Text = "1";
                            break;
                        case 0x01F0:
                            RecordPos = 2;
                            RatLeftSpeed.Text = "2";
                            break;
                        case 0x02E8:
                            RecordPos = 3;
                            RatLeftSpeed.Text = "3";
                            break;
                        case 0x03E1:
                            RecordPos = 4;
                            RatLeftSpeed.Text = "4";
                            break;
                        case 0x04D9:
                            RecordPos = 5;
                            RatLeftSpeed.Text = "5";
                            break;
                        case 0x05D2:
                            RecordPos = 6;
                            RatLeftSpeed.Text = "6";
                            break;
                        case 0x06CA:
                            RecordPos = 7;
                            RatLeftSpeed.Text = "7";
                            break;
                        case 0x07C2:
                            RecordPos = 8;
                            RatLeftSpeed.Text = "8";
                            break;
                        case 0x08BA:
                            RecordPos = 9;
                            RatLeftSpeed.Text = "9";
                            break;
                        case 0x09B2:
                            RecordPos = 10;
                            RatLeftSpeed.Text = "10";
                            break;
                    // POS case 3 deprecated since we changed total POS from 4 to 3 
                    }
        }
        */
        public void Addition(int sensingID, string name)
        {
            Frequency += 1;
        }


        public void XX()//不接收不回傳,內部執行
        {

        }
        public string YY()//()可以限定接收資料及資料型態,並回傳return內的值
        {
            return "";
        }

        

        public string AngleOption()
        {
            return "";
        }
    }
}
