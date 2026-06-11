document.addEventListener('DOMContentLoaded', () => {
    // Restore theme preference from localStorage (default is dark)
    const savedTheme = localStorage.getItem('appTheme');
    if (savedTheme === 'light') {
        document.documentElement.setAttribute('data-theme', 'light');
    } else {
        // Dark theme is default
        document.documentElement.removeAttribute('data-theme');
    }

    // Login handling
    const loginContainer = document.getElementById('loginContainer');
    const dashboardContainer = document.getElementById('dashboardContainer');
    const loginForm = document.getElementById('loginForm');
    const usernameInput = document.getElementById('username');
    const loginError = document.getElementById('loginError');

    // Check if user was already logged in
    if (localStorage.getItem('userLoggedIn') === 'true') {
        loginContainer.style.display = 'none';
        dashboardContainer.style.display = 'flex';
        initializeDashboard();
    }

    loginForm.addEventListener('submit', (e) => {
        e.preventDefault();
        const username = usernameInput.value.trim();

        if (username === 'FUNAAB') {
            localStorage.setItem('userLoggedIn', 'true');
            loginContainer.style.display = 'none';
            dashboardContainer.style.display = 'flex';
            initializeDashboard();
        } else {
            loginError.innerText = '❌ Invalid username. Please try again.';
            usernameInput.value = '';
            usernameInput.focus();
        }
    });

    function initializeDashboard() {
        // UI Elements
        const sidebar = document.getElementById('sidebar');
        const menuToggle = document.getElementById('menuToggle');
        const closeSidebar = document.getElementById('closeSidebar');
        const themeToggle = document.getElementById('themeToggle');
        
        // Initialize theme toggle button text based on current state
        const updateThemeButtonText = () => {
            const isLight = document.documentElement.getAttribute('data-theme') === 'light';
            themeToggle.innerText = isLight ? "🌙 Dark Mode" : "☀️ Light Mode";
        };
        updateThemeButtonText();
        
        const landMap = document.getElementById('landMap');
        const startBtn = document.getElementById('startBtn');
        const stopBtn = document.getElementById('stopBtn');
        const resetBtn = document.getElementById('resetBtn');
        const fieldLengthInput = document.getElementById('fieldLength');
        const fieldWidthInput = document.getElementById('fieldWidth');
        const updateGridBtn = document.getElementById('updateGridBtn');
        const gridInfo = document.getElementById('gridInfo');

        let isPlanting = false;
        let currentStep = 0;
        let battery = 100;
        let obstacleIndices = [];
        let gridRows = 10;
        let gridCols = 10;
        let totalCells = 100;

        const HORIZONTAL_INTERVAL_CM = 30; // 30cm between seeds horizontally
        const VERTICAL_INTERVAL_CM = 60; // 60cm between planting rows vertically


        // 1. Mobile Controls (Fixed Open/Close)
        menuToggle.addEventListener('click', (e) => {
            e.stopPropagation();
            sidebar.classList.add('open');
        });
        closeSidebar.addEventListener('click', () => sidebar.classList.remove('open'));

        // Close sidebar when clicking outside of it
        document.addEventListener('click', (e) => {
            if (sidebar.classList.contains('open') && !sidebar.contains(e.target) && e.target !== menuToggle) {
                setTimeout(() => sidebar.classList.remove('open'), 50);
            }
        });


        // 2. Theme Toggle
        themeToggle.addEventListener('click', () => {
            const isLight = document.documentElement.getAttribute('data-theme') === 'light';
            if (isLight) {
                document.documentElement.removeAttribute('data-theme');
                localStorage.setItem('appTheme', 'dark');
                themeToggle.innerText = "☀️ Light Mode";
            } else {
                document.documentElement.setAttribute('data-theme', 'light');
                localStorage.setItem('appTheme', 'light');
                themeToggle.innerText = "🌙 Dark Mode";
            }
        });

        // Logout button
        const logoutBtn = document.getElementById('logoutBtn');
        logoutBtn.addEventListener('click', () => {
            localStorage.removeItem('userLoggedIn');
            loginContainer.style.display = 'flex';
            dashboardContainer.style.display = 'none';
            usernameInput.value = '';
            loginError.innerText = '';
            isPlanting = false;
            currentStep = 0;
            battery = 100;
            obstacleIndices = [];
            landMap.innerHTML = '';
            document.getElementById('logs').innerHTML = '';
        });


        // 3. Calculate Grid Dimensions
        const calculateGrid = () => {
            const lengthM = parseFloat(fieldLengthInput.value);
            const widthM = parseFloat(fieldWidthInput.value);

            if (!lengthM || !widthM || lengthM <= 0 || widthM <= 0) {
                gridInfo.innerText = "⚠️ Please enter valid dimensions";
                gridInfo.style.color = "#e74c3c";
                return false;
            }

            const lengthCm = lengthM * 100;
            const widthCm = widthM * 100;

            // Calculate rows based on vertical spacing (60cm) and columns based on horizontal spacing (30cm)
            gridRows = Math.ceil(lengthCm / VERTICAL_INTERVAL_CM);
            gridCols = Math.ceil(widthCm / HORIZONTAL_INTERVAL_CM);
            totalCells = gridRows * gridCols;

            gridInfo.innerText = `📐 Grid: ${gridRows} × ${gridCols} = ${totalCells} seeds | Field: ${lengthM}m × ${widthM}m (H:30cm, V:60cm)`;
            gridInfo.style.color = "var(--text)";

            return true;
        };

        // 4. Update Grid
        const updateGrid = () => {
            if (!calculateGrid()) return;

            landMap.innerHTML = '';
            landMap.style.gridTemplateColumns = `repeat(${gridCols}, 1fr)`;
            obstacleIndices = [];
            currentStep = 0;
            battery = 100;
            isPlanting = false;

            for (let i = 0; i < totalCells; i++) {
                const cell = document.createElement('div');
                cell.className = 'cell';
                cell.id = `cell-${i}`;
                landMap.appendChild(cell);
            }

            document.getElementById('powerState').innerText = "IDLE";
            document.getElementById('powerState').className = "badge badge-off";
            document.getElementById('batteryLevel').innerText = "100%";
            document.getElementById('batteryBar').style.width = "100%";
            document.getElementById('plantedCount').innerText = "0";
            document.getElementById('remainingSeeds').innerText = totalCells;
            document.getElementById('obstacleState').innerText = "CLEAR";
            document.getElementById('obstacleState').style.color = "#2ecc71";
            document.getElementById('logs').innerHTML = '';

            addLog(`✅ Grid updated: ${gridRows}×${gridCols} (${totalCells} seeds total)`);
            sidebar.classList.remove('open');
        };

        updateGridBtn.addEventListener('click', updateGrid);
        fieldWidthInput.addEventListener('keypress', (e) => {
            if (e.key === 'Enter') updateGrid();
        });

        // 5. Linear Serpentine Path Logic
        // Maps step index to the correct cell index for S-curve
        const getSerpentineIndex = (step) => {
            let row = Math.floor(step / gridCols);
            let col = step % gridCols;
            // If row is odd (1, 3, 5...), reverse the column direction
            if (row % 2 !== 0) {
                return (row * gridCols) + (gridCols - 1 - col);
            }
            return (row * gridCols) + col;
        };


        // 6. Generate obstacles between 3% and 10% of total area
        const generateObstacles = () => {
            obstacleIndices = [];
            // Calculate random percentage between 3% and 10%
            const randomPercent = 3 + Math.random() * 7; // 3 to 10
            const numObstacles = Math.ceil(totalCells * (randomPercent / 100));
            
            while(obstacleIndices.length < numObstacles) {
                let r = Math.floor(Math.random() * (totalCells - 2)) + 1;
                if(!obstacleIndices.includes(r)) obstacleIndices.push(r);
            }
        };


        const runMission = () => {
            if(!isPlanting || currentStep >= totalCells) {
                if(currentStep >= totalCells) addLog("🎉 Mission Completed Successfully!");
                stopMission();
                return;
            }

            const cellIdx = getSerpentineIndex(currentStep);
            const cell = document.getElementById(`cell-${cellIdx}`);
            const obsEl = document.getElementById('obstacleState');

            // Check for obstacle
            if (obstacleIndices.includes(cellIdx)) {
                cell.classList.add('obstacle');
                obsEl.innerText = "DETECTED";
                obsEl.style.color = "#e74c3c";
                addLog(`⚠️ Obstacle detected at Cell ${cellIdx}. Re-routing...`);
                
                // Wait 2 seconds then skip
                setTimeout(() => {
                    obsEl.innerText = "CLEAR";
                    obsEl.style.color = "#2ecc71";
                    currentStep++;
                    runMission();
                }, 2000);
            } else {
                // Sowing logic
                cell.classList.add('planted');
                battery -= calculateBatteryDrain();
                updateUI();
                currentStep++;
                setTimeout(runMission, 1000);
            }
        };

        const startMission = () => {
            if(isPlanting) return;
            if(totalCells === 0) {
                addLog("❌ Please configure grid first!");
                return;
            }
            generateObstacles();
            isPlanting = true;
            document.getElementById('powerState').innerText = "PLANTING";
            document.getElementById('powerState').className = "badge badge-on";
            addLog("🚀 Mission Started: Serpentine path engaged.");
            runMission();
        };

        const stopMission = () => {
            isPlanting = false;
            document.getElementById('powerState').innerText = "IDLE";
            document.getElementById('powerState').className = "badge badge-off";
            addLog("🛑 System Halted.");
        };

        const resetSystem = () => {
            isPlanting = false;
            currentStep = 0;
            battery = 100;
            obstacleIndices = [];
            fieldLengthInput.value = "10";
            fieldWidthInput.value = "10";
            
            updateGrid();
            addLog("🔄 System Reset: All settings restored to default.");
        };

        const updateUI = () => {
            document.getElementById('batteryBar').style.width = Math.max(0, battery) + "%";
            document.getElementById('batteryLevel').innerText = Math.round(Math.max(0, battery)) + "%";
            let planted = document.querySelectorAll('.cell.planted').length;
            document.getElementById('plantedCount').innerText = planted;
            document.getElementById('remainingSeeds').innerText = totalCells - planted;
        };

        function addLog(msg) {
            const logs = document.getElementById('logs');
            const li = document.createElement('li');
            li.innerHTML = `<small>${new Date().toLocaleTimeString()}</small> ${msg}`;
            logs.prepend(li);
        }

        // Calculate battery drain: 100% for 500 square meters
        // Each cell covers (30cm × 60cm) = 0.18 square meters
        const calculateBatteryDrain = () => {
            const cellAreaSqMeters = (HORIZONTAL_INTERVAL_CM / 100) * (VERTICAL_INTERVAL_CM / 100);
            const cellsPerTarget = 500 / cellAreaSqMeters;
            return 100 / cellsPerTarget;
        };

        startBtn.addEventListener('click', startMission);
        stopBtn.addEventListener('click', stopMission);
        resetBtn.addEventListener('click', resetSystem);

        // Initialize default grid
        updateGrid();
    }
});