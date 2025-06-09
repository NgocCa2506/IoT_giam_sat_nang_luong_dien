import { initializeApp } from "https://www.gstatic.com/firebasejs/11.0.2/firebase-app.js";
import { getDatabase, ref, onValue, set, remove } from "https://www.gstatic.com/firebasejs/11.0.2/firebase-database.js";

// Cấu hình Firebase
const firebaseConfig = {
    apiKey: "AIzaSyAtIdRjaNlEwuTeI3kdyZNMmZY4xKTID6M",
    authDomain: "dacs4pzem004t.firebaseapp.com",
    databaseURL: "https://dacs4pzem004t-default-rtdb.asia-southeast1.firebasedatabase.app",
    projectId: "dacs4pzem004t",
    storageBucket: "dacs4pzem004t.appspot.com",
    messagingSenderId: "493460816989",
    appId: "1:493460816989:web:91508dd97f5824f4eff9b4",
    measurementId: "G-Z1RSS21XCX"
};

// Khởi tạo Firebase
const app = initializeApp(firebaseConfig);
const database = getDatabase(app);

// DOM Elements
const energyToday = document.getElementById("energyToday");
const energyYesterday = document.getElementById("energyYesterday");
const energyThisMonth = document.getElementById("energyThisMonth");
const energyLastMonth = document.getElementById("energyLastMonth");
const costThisMonth = document.getElementById("costThisMonth");
const costLastMonth = document.getElementById("costLastMonth");
const voltage = document.getElementById("voltage");
const current = document.getElementById("current");
const power = document.getElementById("power");
const temperature = document.getElementById("temperature");
const humidity = document.getElementById("humidity");
const relayButton = document.getElementById("relayButton");
const relayScheduleButton = document.getElementById("relayScheduleButton");
const scheduleFeedback = document.getElementById("scheduleFeedback");
const relayScheduleForm = document.getElementById("relayScheduleForm");
const lastUpdated = document.getElementById("lastUpdated");

// Cờ kiểm soát điều khiển thủ công và lịch
let manualControl = false;

// Hàm lấy dữ liệu từ Firebase và hiển thị lên giao diện
function fetchData() {
    const readingsRef = ref(database, "readings");
    onValue(readingsRef, (snapshot) => {
        const data = snapshot.val();
        if (data) {
            energyToday.textContent = `${data.energy_Today || 0} kWh`;
            energyYesterday.textContent = `${data.energy_Yesterday || 0} kWh`;
            energyThisMonth.textContent = `${data.energy_This_Month || 0} kWh`;
            energyLastMonth.textContent = `${data.energy_Last_Month || 0} kWh`;
            costThisMonth.textContent = `${data.cost_This_Month || 0} VND`;
            costLastMonth.textContent = `${data.cost_Last_Month || 0} VND`;
            voltage.textContent = `${data.dienAp || 0} V`;
            current.textContent = `${data.dongDien || 0} A`;
            power.textContent = `${data.congSuat || 0} W`;
            temperature.textContent = `${data.nhietDo || 0} °C`;
            humidity.textContent = `${data.doAm || 0} %`;
        }
    });

    // Cập nhật trạng thái relay
    const relayRef = ref(database, "relay/state");
    onValue(relayRef, (snapshot) => {
        const relayState = snapshot.val();
        updateRelayButton(relayState);
    });
}

// Hàm cập nhật giao diện nút relay
function updateRelayButton(state) {
    if (state === 1) {
        relayButton.textContent = "RELAY ĐANG TẮT";
        relayButton.classList.add("off");
        relayButton.classList.remove("on");
    } else {
        relayButton.textContent = "RELAY ĐANG BẬT";
        relayButton.classList.add("on");
        relayButton.classList.remove("off");
    }
}

// Hàm bật/tắt relay
relayButton.addEventListener("click", () => {
    toggleRelayState();
});

// Hàm thay đổi trạng thái relay
function toggleRelayState() {
    const relayRef = ref(database, "relay/state");
    const currentState = relayButton.textContent === "RELAY ĐANG TẮT" ? 1 : 0;
    const newState = currentState === 1 ? 0 : 1;

    // Điều khiển thủ công, xóa lịch bật tắt
    manualControl = true;
    remove(ref(database, "relay/schedule"))
        .catch(error => console.error("Error removing schedule:", error));

    // Cập nhật trạng thái relay
    set(relayRef, newState)
        .then(() => {
            updateRelayButton(newState);
        })
        .catch(error => {
            console.error("Error updating relay state:", error);
        })
        .finally(() => {
            setTimeout(() => manualControl = false, 1000);
        });
}

// Hàm lưu lịch bật/tắt relay vào Firebase
relayScheduleForm.addEventListener("submit", (event) => {
    event.preventDefault();

    const date = document.getElementById("date").value;
    const timeOn = document.getElementById("timeOn").value;
    const timeOff = document.getElementById("timeOff").value;

    if (date && timeOn && timeOff) {
        const scheduleRef = ref(database, "relay/schedule");
        const scheduleData = { date, timeOn, timeOff };

        // Lưu lịch vào Firebase
        set(scheduleRef, scheduleData)
            .then(() => {
                scheduleFeedback.textContent = "Lịch đã được đặt thành công!";
                scheduleFeedback.style.color = "green";
                relayScheduleButton.textContent = "Hủy lịch";
                // Ẩn nút "Đặt lịch" khi lịch đã được đặt
                document.getElementById("setScheduleButton").style.display = "none";
                // Hiển thị nút "Hủy lịch"
                relayScheduleButton.style.display = "inline-block";
                manualControl = false;
            })
            .catch((error) => {
                scheduleFeedback.textContent = "Đặt lịch thất bại!";
                scheduleFeedback.style.color = "red";
                console.error("Error setting schedule:", error);
            });
    } else {
        scheduleFeedback.textContent = "Vui lòng nhập đầy đủ thông tin!";
        scheduleFeedback.style.color = "red";
    }
});

// Hàm hủy lịch bật/tắt relay
relayScheduleButton.addEventListener("click", () => {
    const scheduleRef = ref(database, "relay/schedule");
    remove(scheduleRef)
        .then(() => {
            scheduleFeedback.textContent = "Lịch đã bị hủy!";
            scheduleFeedback.style.color = "red";
            relayScheduleButton.textContent = "Đặt lịch";
            // Hiển thị lại nút "Đặt lịch" khi lịch đã bị hủy
            document.getElementById("setScheduleButton").style.display = "inline-block";
            // Ẩn nút "Hủy lịch"
            relayScheduleButton.style.display = "none";
        })
        .catch((error) => {
            scheduleFeedback.textContent = "Lỗi khi hủy lịch!";
            scheduleFeedback.style.color = "red";
            console.error("Error removing schedule:", error);
        });
});

// Hàm kiểm tra và điều khiển relay theo lịch
function checkRelaySchedule() {
    if (manualControl) return; // Nếu đang điều khiển thủ công, bỏ qua kiểm tra lịch

    const scheduleRef = ref(database, "relay/schedule");
    onValue(scheduleRef, (snapshot) => {
        const scheduleData = snapshot.val();
        if (scheduleData) {
            const { date, timeOn, timeOff } = scheduleData;
            const scheduleDate = new Date(`${date}T${timeOn}:00`);
            const offDate = new Date(`${date}T${timeOff}:00`);
            const now = new Date();

            if (now >= scheduleDate && now <= offDate) {
                set(ref(database, "relay/state"), 0); // Bật relay
            } else if (now > offDate) {
                set(ref(database, "relay/state"), 1) // Tắt relay
                    .then(() => {
                        // Xóa nhánh `schedule` sau khi tắt relay
                        remove(scheduleRef)
                            .then(() => {
                                console.log("Lịch đã bị xóa sau khi hết thời gian tắt.");
                            })
                            .catch((error) => {
                                console.error("Lỗi khi xóa lịch:", error);
                            });
                    });
            }
        }
    });
}

// Kiểm tra và cập nhật trạng thái relay mỗi giây
setInterval(checkRelaySchedule, 1000);

// Cập nhật thời gian mỗi giây
function updateTime() {
    const now = new Date();
    lastUpdated.textContent = `${now.getDate()}-${now.getMonth() + 1}-${now.getFullYear()} ${now.getHours()}:${now.getMinutes()}:${now.getSeconds()}`;
}

setInterval(updateTime, 1000);

// Khởi động chương trình
fetchData();
updateTime();
