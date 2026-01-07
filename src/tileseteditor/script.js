class TilesetEditor {
    constructor() {
        this.tileset = null;
        this.tileWidth = 32;
        this.tileHeight = 32;
        this.selectedTile = null;
        this.mapWidth = 20;
        this.mapHeight = 15;
        this.mapData = [];
        this.selectionStart = null;
        this.selectionCurrent = null;
        this.selectionActive = false;
        
        this.tilesetCanvas = document.getElementById('tilesetCanvas');
        this.tilesetCtx = this.tilesetCanvas.getContext('2d');
        this.mapCanvas = document.getElementById('mapCanvas');
        this.mapCtx = this.mapCanvas.getContext('2d');
        
        this.initEventListeners();
    }
    
    initEventListeners() {
        document.getElementById('loadTileset').addEventListener('click', () => this.loadTileset());
        document.getElementById('createMap').addEventListener('click', () => this.createMap());
        document.getElementById('clearMap').addEventListener('click', () => this.clearMap());
        document.getElementById('exportMap').addEventListener('click', () => this.exportMap());
        
        this.tilesetCanvas.addEventListener('click', (e) => this.selectTile(e));
        this.mapCanvas.addEventListener('mousedown', (e) => this.onMapMouseDown(e));
        this.mapCanvas.addEventListener('mousemove', (e) => this.onMapMouseMove(e));
        this.mapCanvas.addEventListener('mouseup', (e) => this.onMapMouseUp(e));
    }
    
    loadTileset() {
        const fileInput = document.getElementById('tilesetInput');
        const file = fileInput.files[0];
        
        if (!file) {
            alert('Please select a tileset image first');
            return;
        }
        
        this.tileWidth = parseInt(document.getElementById('tileWidth').value);
        this.tileHeight = parseInt(document.getElementById('tileHeight').value);
        
        const reader = new FileReader();
        reader.onload = (e) => {
            const img = new Image();
            img.onload = () => {
                this.tileset = img;
                this.renderTileset();
            };
            img.src = e.target.result;
        };
        reader.readAsDataURL(file);
    }
    
    renderTileset() {
        if (!this.tileset) return;
        
        this.tilesetCanvas.width = this.tileset.width;
        this.tilesetCanvas.height = this.tileset.height;
        this.tilesetCtx.drawImage(this.tileset, 0, 0);
        
        // Draw grid
        this.tilesetCtx.strokeStyle = 'rgba(255, 255, 255, 0.3)';
        this.tilesetCtx.lineWidth = 1;
        
        for (let x = 0; x < this.tileset.width; x += this.tileWidth) {
            this.tilesetCtx.beginPath();
            this.tilesetCtx.moveTo(x, 0);
            this.tilesetCtx.lineTo(x, this.tileset.height);
            this.tilesetCtx.stroke();
        }
        
        for (let y = 0; y < this.tileset.height; y += this.tileHeight) {
            this.tilesetCtx.beginPath();
            this.tilesetCtx.moveTo(0, y);
            this.tilesetCtx.lineTo(this.tileset.width, y);
            this.tilesetCtx.stroke();
        }
    }
    
    selectTile(e) {
        if (!this.tileset) return;
        
        const rect = this.tilesetCanvas.getBoundingClientRect();
        const x = Math.floor((e.clientX - rect.left) / this.tileWidth);
        const y = Math.floor((e.clientY - rect.top) / this.tileHeight);
        
        this.selectedTile = { x, y };
        
        this.renderTileset();
        
        // Highlight selected tile
        this.tilesetCtx.strokeStyle = 'yellow';
        this.tilesetCtx.lineWidth = 2;
        this.tilesetCtx.strokeRect(
            x * this.tileWidth,
            y * this.tileHeight,
            this.tileWidth,
            this.tileHeight
        );
        
        document.getElementById('selectedTile').textContent = 
            `Selected: Tile (${x}, ${y})`;
    }
    
    createMap() {
        this.mapWidth = parseInt(document.getElementById('mapWidth').value);
        this.mapHeight = parseInt(document.getElementById('mapHeight').value);
        
        this.mapData = Array(this.mapHeight).fill(null).map(() => 
            Array(this.mapWidth).fill(null)
        );
        
        this.mapCanvas.width = this.mapWidth * this.tileWidth;
        this.mapCanvas.height = this.mapHeight * this.tileHeight;
        
        this.renderMap();
    }
    
    clearMap() {
        if (!this.mapData.length) return;
        
        this.mapData = this.mapData.map(row => row.map(() => null));
        this.renderMap();
    }
    
    onMapMouseDown(e) {
        if (!this.selectedTile || !this.mapData.length || !this.tileset) return;

        const { x, y } = this.getGridFromEvent(e);
        if (x === null) return;

        if (e.shiftKey) {
            this.selectionStart = { x, y };
            this.selectionCurrent = { x, y };
            this.selectionActive = true;
            this.renderMap();
            return;
        }

        this.applyTileAt(x, y);
    }

    onMapMouseMove(e) {
        if (this.selectionActive) {
            const { x, y } = this.getGridFromEvent(e);
            if (x !== null) {
                this.selectionCurrent = { x, y };
                this.renderMap();
            }
            return;
        }

        if (e.buttons === 1) {
            const { x, y } = this.getGridFromEvent(e);
            if (x !== null) {
                this.applyTileAt(x, y);
            }
        }
    }

    onMapMouseUp(e) {
        if (!this.selectionActive) return;

        const { x, y } = this.getGridFromEvent(e);
        if (x !== null) {
            this.selectionCurrent = { x, y };
            this.fillSelection();
        }

        this.selectionActive = false;
        this.selectionStart = null;
        this.selectionCurrent = null;
        this.renderMap();
    }

    getGridFromEvent(e) {
        const rect = this.mapCanvas.getBoundingClientRect();
        const x = Math.floor((e.clientX - rect.left) / this.tileWidth);
        const y = Math.floor((e.clientY - rect.top) / this.tileHeight);

        if (x < 0 || x >= this.mapWidth || y < 0 || y >= this.mapHeight) {
            return { x: null, y: null };
        }
        return { x, y };
    }

    applyTileAt(x, y) {
        this.mapData[y][x] = { ...this.selectedTile };
        this.renderMap();
    }

    fillSelection() {
        if (!this.selectionStart || !this.selectionCurrent) return;

        const x1 = Math.min(this.selectionStart.x, this.selectionCurrent.x);
        const x2 = Math.max(this.selectionStart.x, this.selectionCurrent.x);
        const y1 = Math.min(this.selectionStart.y, this.selectionCurrent.y);
        const y2 = Math.max(this.selectionStart.y, this.selectionCurrent.y);

        for (let y = y1; y <= y2; y++) {
            for (let x = x1; x <= x2; x++) {
                this.mapData[y][x] = { ...this.selectedTile };
            }
        }
    }
    
    renderMap() {
        this.mapCtx.fillStyle = '#1e1e1e';
        this.mapCtx.fillRect(0, 0, this.mapCanvas.width, this.mapCanvas.height);
        
        // Draw grid
        this.mapCtx.strokeStyle = 'rgba(255, 255, 255, 0.1)';
        this.mapCtx.lineWidth = 1;
        
        for (let x = 0; x <= this.mapWidth; x++) {
            this.mapCtx.beginPath();
            this.mapCtx.moveTo(x * this.tileWidth, 0);
            this.mapCtx.lineTo(x * this.tileWidth, this.mapCanvas.height);
            this.mapCtx.stroke();
        }
        
        for (let y = 0; y <= this.mapHeight; y++) {
            this.mapCtx.beginPath();
            this.mapCtx.moveTo(0, y * this.tileHeight);
            this.mapCtx.lineTo(this.mapCanvas.width, y * this.tileHeight);
            this.mapCtx.stroke();
        }
        
        // Draw tiles
        if (!this.tileset) return;
        
        for (let y = 0; y < this.mapHeight; y++) {
            for (let x = 0; x < this.mapWidth; x++) {
                const tile = this.mapData[y][x];
                if (tile) {
                    this.mapCtx.drawImage(
                        this.tileset,
                        tile.x * this.tileWidth,
                        tile.y * this.tileHeight,
                        this.tileWidth,
                        this.tileHeight,
                        x * this.tileWidth,
                        y * this.tileHeight,
                        this.tileWidth,
                        this.tileHeight
                    );
                }
            }
        }

        if (this.selectionActive && this.selectionStart && this.selectionCurrent) {
            const x1 = Math.min(this.selectionStart.x, this.selectionCurrent.x);
            const x2 = Math.max(this.selectionStart.x, this.selectionCurrent.x);
            const y1 = Math.min(this.selectionStart.y, this.selectionCurrent.y);
            const y2 = Math.max(this.selectionStart.y, this.selectionCurrent.y);

            this.mapCtx.strokeStyle = 'rgba(255, 255, 0, 0.9)';
            this.mapCtx.lineWidth = 2;
            this.mapCtx.strokeRect(
                x1 * this.tileWidth + 0.5,
                y1 * this.tileHeight + 0.5,
                (x2 - x1 + 1) * this.tileWidth,
                (y2 - y1 + 1) * this.tileHeight
            );

            this.mapCtx.fillStyle = 'rgba(255, 255, 0, 0.1)';
            this.mapCtx.fillRect(
                x1 * this.tileWidth,
                y1 * this.tileHeight,
                (x2 - x1 + 1) * this.tileWidth,
                (y2 - y1 + 1) * this.tileHeight
            );
        }
    }
    
    exportMap() {
        if (!this.mapData.length) {
            alert('Create a map first');
            return;
        }
        
        const exportData = {
            tileWidth: this.tileWidth,
            tileHeight: this.tileHeight,
            mapWidth: this.mapWidth,
            mapHeight: this.mapHeight,
            tiles: this.mapData
        };
        
        const dataStr = JSON.stringify(exportData, null, 2);
        const dataBlob = new Blob([dataStr], { type: 'application/json' });
        const url = URL.createObjectURL(dataBlob);
        
        const link = document.createElement('a');
        link.href = url;
        link.download = 'tilemap.json';
        link.click();
        
        URL.revokeObjectURL(url);
    }
}

// Initialize editor
const editor = new TilesetEditor();