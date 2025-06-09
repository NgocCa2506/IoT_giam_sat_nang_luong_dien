#include <WiFi.h>
#include <FirebaseESP32.h>
#include <Adafruit_SSD1306.h>
#include <PZEM004Tv30.h>
#include <DHT.h>
#include <TimeLib.h> 
#include <NTPClient.h>
#include <WiFiUdp.h>

// Thông tin Wi-Fi
#define WIFI_SSID "le thu thao"
#define WIFI_PASSWORD "khaidong3"

// Thông tin Firebase
#define FIREBASE_HOST "https://dacs4pzem004t-default-rtdb.asia-southeast1.firebasedatabase.app"
#define FIREBASE_AUTH "idg5H9yS6BphSw98DVRXfQSqV0V1C3EYcZZPHjNv"

// Cấu hình màn hình OLED
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 manHinh(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// Cấu hình PZEM-004T
#define PZEM_RX_PIN 16
#define PZEM_TX_PIN 17
HardwareSerial PZEMSerial(2);
PZEM004Tv30 pzem(&PZEMSerial, PZEM_RX_PIN, PZEM_TX_PIN);

// Cấu hình DHT22
#define DHT_PIN 19
#define DHT_TYPE DHT22
DHT dht(DHT_PIN, DHT_TYPE);

// Cài đặt relay
#define RELAY_PIN 18
bool relayState = false;

// NTP Client
WiFiUDP ntpUDP;
NTPClient dongHo(ntpUDP, "pool.ntp.org", 7 * 3600, 60000); // GMT+7

// Cấu hình Firebase
FirebaseData firebaseData;
FirebaseConfig firebaseCauHinh;
FirebaseAuth firebaseXacThuc;

// Biến dữ liệu
float dienAp, dongDien, congSuat, nangLuong;
float nhietDo, doAm;
float nangLuongNgay = 0; 
float nangLuongCu = 0;
float nangLuongThang = 0;
float nangLuongHomQua = 0;  
float nangLuongThangTruoc = 0;  

// Giá điện
int giaMua1 = 1806, giaMua2 = 1866, giaMua3 = 2167;
int giaMua4 = 2729, giaMua5 = 3050, giaMua6 = 3151;

// Nút chuyển trang
#define BUTTON_PIN 14
int trangThaiNut = 0;
int trangThaiNutTruoc = 0;
int trangHienTai = 1; 

// Hàm tính chi phí điện năng tiêu thụ
int tinhChiPhiDien(float nangLuongTieuThu) {
    int chiPhi = 0;
    if (nangLuongTieuThu <= 50) {
        chiPhi = nangLuongTieuThu * giaMua1;
    } else if (nangLuongTieuThu <= 100) {
        chiPhi = 50 * giaMua1 + (nangLuongTieuThu - 50) * giaMua2;
    } else if (nangLuongTieuThu <= 200) {
        chiPhi = 50 * giaMua1 + 50 * giaMua2 + (nangLuongTieuThu - 100) * giaMua3;
    } else if (nangLuongTieuThu <= 300) {
        chiPhi = 50 * giaMua1 + 50 * giaMua2 + 100 * giaMua3 + (nangLuongTieuThu - 200) * giaMua4;
    } else if (nangLuongTieuThu <= 400) {
        chiPhi = 50 * giaMua1 + 50 * giaMua2 + 100 * giaMua3 + 100 * giaMua4 + (nangLuongTieuThu - 300) * giaMua5;
    } else {
        chiPhi = 50 * giaMua1 + 50 * giaMua2 + 100 * giaMua3 + 100 * giaMua4 + 100 * giaMua5 + (nangLuongTieuThu - 400) * giaMua6;
    }
    return chiPhi;
}

// Hàm kiểm tra ngày mới để reset năng lượng
int ngayHomQua = -1;

void kiemTraNgayMoi() {
  time_t now = dongHo.getEpochTime();
  int ngayHienTai = day(now);
  // Kiểm tra nếu ngày hiện tại khác với ngày hôm qua
  if (ngayHienTai != ngayHomQua) {
    nangLuongHomQua = nangLuongNgay;
    nangLuongNgay = 0;
    ngayHomQua = ngayHienTai;
  }
}

// Hàm kiểm tra tháng mới để reset năng lượng tháng
void kiemTraThangMoi() {
    time_t now = dongHo.getEpochTime();
    static int thangTruocDo = -1;
    int ngay = day(now);
    int thang = month(now);
    int nam = year(now);

    Serial.println("Ngay: " + String(ngay) + " / " + String(thang) + " / " + String(nam));

    String path = "/readings/" + String(millis());

    // Kiểm tra nếu đã qua tháng mới
    if (thang != thangTruocDo && thangTruocDo != -1) {
        // Cập nhật chi phí tháng trước và năng lượng tháng trước lên Firebase
        int tienDienThangTruoc = tinhChiPhiDien(nangLuongThang);
        Firebase.setFloat(firebaseData, path + "/cost_Last_Month", tienDienThangTruoc);
        Firebase.setFloat(firebaseData, path + "/energy_Last_Month", nangLuongThang);

        // Cập nhật năng lượng tháng này và reset năng lượng tháng
        nangLuongThangTruoc = nangLuongThang;
        nangLuongThang = 0; // Reset năng lượng tháng
        Firebase.setFloat(firebaseData, path + "/energy_This_Month", nangLuongThang);
        Firebase.setFloat(firebaseData, path + "/cost_This_Month", 0);  // Đặt chi phí tháng này là 0
    }
    thangTruocDo = thang;  // Lưu tháng hiện tại
}

void setup() {
    Serial.begin(115200);

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.println("Dang ket noi Wi-Fi...");
    }
    Serial.println("Da ket noi Wi-Fi!");

    dongHo.begin();
    dongHo.update();

    firebaseCauHinh.database_url = FIREBASE_HOST;
    firebaseCauHinh.signer.tokens.legacy_token = FIREBASE_AUTH;
    Firebase.begin(&firebaseCauHinh, &firebaseXacThuc);
    Firebase.reconnectWiFi(true);

    PZEMSerial.begin(9600, SERIAL_8N1, PZEM_RX_PIN, PZEM_TX_PIN);

    dht.begin();

    if (!manHinh.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println(F("Khong khoi tao duoc man hinh OLED"));
        for (;;);
    }
    manHinh.clearDisplay();

    pinMode(RELAY_PIN, OUTPUT);
    digitalWrite(RELAY_PIN, LOW);

    pinMode(BUTTON_PIN, INPUT_PULLUP);
}

void loop() {
  dongHo.update();

  trangThaiNut = digitalRead(BUTTON_PIN);

  if (trangThaiNut == LOW && trangThaiNutTruoc == HIGH) {
      delay(200);  
      trangHienTai = (trangHienTai == 1) ? 2 : 1;  
  }
  trangThaiNutTruoc = trangThaiNut;

  if (trangHienTai == 1) {
      hienThiTrang1();
  } else {
      hienThiTrang2();
  }

  dienAp = pzem.voltage();
  dongDien = pzem.current();
  congSuat = pzem.power();
  nangLuong = pzem.energy();
  nhietDo = dht.readTemperature();
  doAm = dht.readHumidity();

  // Đọc năng lượng mới từ cảm biến
  float nangLuongMoi = pzem.energy();

  // Nếu năng lượng mới hợp lệ, tính toán sự thay đổi
  if (!isnan(nangLuongMoi)) {
    float deltaEnergy = nangLuongMoi - nangLuongCu;

    // Nếu có sự thay đổi dương (năng lượng tiêu thụ)
    if (deltaEnergy > 0) {
        nangLuongNgay += deltaEnergy;  // Cộng dồn năng lượng hôm nay
        nangLuongThang += deltaEnergy; // Cộng dồn năng lượng tháng
    }

    // Cập nhật giá trị năng lượng cũ
    nangLuongCu = nangLuongMoi;
  }

  kiemTraNgayMoi();  // Kiểm tra ngày mới để reset năng lượng hôm nay
  kiemTraThangMoi();  // Kiểm tra tháng mới để reset năng lượng tháng

  if (Firebase.ready()) {
    String path = "/readings";
    //Firebase.setFloat(firebaseData, path + "/energy", nangLuong);
    Firebase.setFloat(firebaseData, path + "/energy_Today", nangLuongNgay);
    Firebase.setFloat(firebaseData, path + "/energy_Yesterday", nangLuongHomQua);
    Firebase.setFloat(firebaseData, path + "/energy_This_Month", nangLuongThang);
    Firebase.setFloat(firebaseData, path + "/energy_Last_Month", nangLuongThangTruoc);
    Firebase.setInt(firebaseData, path + "/cost_This_Month", tinhChiPhiDien(nangLuongThang));
    Firebase.setInt(firebaseData, path + "/cost_Last_Month", tinhChiPhiDien(nangLuongThangTruoc));

    if (congSuat > 100) {
      Firebase.setInt(firebaseData, "/relay/state", 0); // Relay OFF
    } 

    //Firebase.setString(firebaseData, "/relay/state", relayState ? "ON" : "OFF");
    if (Firebase.getInt(firebaseData, "/relay/state")) {
      relayState = firebaseData.intData() == 1;
      digitalWrite(RELAY_PIN, relayState ? HIGH : LOW);
    }

    Firebase.setFloat(firebaseData, path + "/congSuat", congSuat);
    Firebase.setFloat(firebaseData, path + "/dienAp", dienAp);
    Firebase.setFloat(firebaseData, path + "/dongDien", dongDien);
    Firebase.setFloat(firebaseData, path + "/nhietDo", nhietDo);
    Firebase.setFloat(firebaseData, path + "/doAm", doAm);
  }

  delay(3000); 
}

void hienThiTrang1() {
  manHinh.clearDisplay();
  manHinh.setTextColor(WHITE);
  time_t now = dongHo.getEpochTime();
  int ngay = day(now);
  int thang = month(now);
  int nam = year(now);

  manHinh.setCursor(0, 0);
  manHinh.setTextSize(2);
  manHinh.print(String(ngay) + "/" + String(thang) + "/" + String(nam));

  // Hiển thị điện áp
  manHinh.setCursor(0, 20);
  manHinh.setTextSize(1);
  manHinh.print("Dien ap: ");
  if (isnan(dienAp)) {
    manHinh.print("N/A");
  } else {
    manHinh.print(dienAp, 1);
    manHinh.print(" V");
  }

  // Hiển thị dòng điện
  manHinh.setCursor(0, 30);
  manHinh.setTextSize(1);
  manHinh.print("Dong dien: ");
  if (isnan(dongDien)) {
    manHinh.print("N/A");
  } else {
    manHinh.print(dongDien, 2); // 2 chữ số thập phân
    manHinh.print(" A");
  }

  // Hiển thị công suất
  manHinh.setCursor(0, 40);
  manHinh.setTextSize(1);
  manHinh.print("Cong suat: ");
  if (isnan(congSuat)) {
      manHinh.print("N/A");
  } else {
      manHinh.print(congSuat, 1);
      manHinh.print(" W");
  }

  //Hiển thị trạng thái của relay
  manHinh.setCursor(0, 50);
  manHinh.setTextSize(1);
  int trangThaiRelay = digitalRead(RELAY_PIN);
  if (trangThaiRelay == HIGH) {
      manHinh.print("Relay: OFF");
  } else {
      manHinh.print("Relay: ON");
  }
  manHinh.display();
}

// Trang 2
void hienThiTrang2() {
  manHinh.clearDisplay();
  manHinh.setTextSize(1);
  manHinh.setTextColor(WHITE);

  manHinh.setCursor(0, 0);
  manHinh.print("Hom nay: ");
  manHinh.print(isnan(nangLuongNgay) ? "N/A" : String(nangLuongNgay, 2) + " kWh");

  manHinh.setCursor(0, 10);
  manHinh.print("Hom qua: ");
  manHinh.print(isnan(nangLuongHomQua) ? "N/A" : String(nangLuongHomQua, 2) + " kWh");

  manHinh.setCursor(0, 20);
  manHinh.print("Thang nay: ");
  manHinh.print(isnan(nangLuongThang) ? "N/A" : String(nangLuongThang, 2) + " kWh");

  manHinh.setCursor(0, 30);
  manHinh.print("Tien T.N: ");
  int chiPhiThangNay = tinhChiPhiDien(nangLuongThang);
  manHinh.print(isnan(chiPhiThangNay) ? "N/A" : String(chiPhiThangNay) + " VND");

  manHinh.setCursor(0, 40);
  manHinh.print("Thang truoc: ");
  manHinh.print(isnan(nangLuongThangTruoc) ? "N/A" : String(nangLuongThangTruoc, 2) + " kWh");

  manHinh.setCursor(0, 50);
  manHinh.print("Tien T.T: ");
  int chiPhiThangTruoc = tinhChiPhiDien(nangLuongThangTruoc);
  manHinh.print(isnan(chiPhiThangTruoc) ? "N/A" : String(chiPhiThangTruoc) + " VND");

  manHinh.display();
}