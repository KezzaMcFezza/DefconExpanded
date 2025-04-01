//DefconExpanded, Created by...
//KezzaMcFezza - Main Developer
//Nexustini - Server Managment
//
//Notable Mentions...
//Rad - For helping with python scripts.
//Bert_the_turtle - Doing everthing with c++
//
//Inspired by Sievert and Wan May
// 
//Last Edited 01-04-2025

function initializeProfileEditing() {
    const editButton = document.getElementById('edit-profile-btn');
    const editButtonMobile = document.getElementById('edit-profile-btn-mobile');
    const editableFields = document.querySelectorAll('.editable');
    const editableLists = document.querySelectorAll('.editable-list');
    let isEditing = false;

    if (editButton) {
        editButton.addEventListener('click', toggleEdit);
    }

    if (editButtonMobile) {
        editButtonMobile.addEventListener('click', toggleEdit);
    }

    function toggleEdit() {
        isEditing = !isEditing;
        
        document.body.classList.toggle('editing');
        
        void document.body.offsetHeight;
        
        if (editButton) {
            editButton.classList.toggle('editing');
            editButton.innerHTML = isEditing ?
                '<i class="fas fa-save"></i> Save Changes' :
                '<i class="fas fa-pencil-alt"></i> Edit Profile';
        }

        if (editButtonMobile) {
            editButtonMobile.classList.toggle('editing');
            editButtonMobile.innerHTML = isEditing ?
                '<i class="fas fa-save"></i> Save Changes' :
                '<i class="fas fa-pencil-alt"></i> Edit Profile';
        }

        if (isEditing) {
            
            setTimeout(enableEditing, 0);
        } else {
            saveChanges();
        }
        
        editableFields.forEach(field => {
            if (isEditing) {
                field.setAttribute('data-editing', 'true');
            } else {
                field.removeAttribute('data-editing');
            }
        });
    }

    function enableEditing() {
        editableFields.forEach(field => {
            field.contentEditable = true;
            
            field.classList.add('temp-class');
            void field.offsetHeight; 
            field.classList.remove('temp-class');
        });

        editableLists.forEach(list => {
            const addItemBtn = document.createElement('button');
            addItemBtn.textContent = 'Add Item';
            addItemBtn.classList.add('add-item-btn');
            addItemBtn.addEventListener('click', (e) => {
                e.preventDefault();
                addListItem(list);
            });
            list.parentNode.insertBefore(addItemBtn, list.nextSibling);

            list.querySelectorAll('li').forEach(item => addDeleteButton(item));
        });
        
        if (!document.getElementById('edit-mode-styles')) {
            const style = document.createElement('style');
            style.id = 'edit-mode-styles';
            style.textContent = `
                body.editing .editable {
                    background-color: rgba(77, 166, 255, 0.1) !important;
                    outline: 1px dashed rgba(77, 166, 255, 0.5) !important;
                    cursor: text !important;
                }
                [data-editing="true"] {
                    background-color: rgba(77, 166, 255, 0.1) !important;
                    outline: 1px dashed rgba(77, 166, 255, 0.5) !important;
                    cursor: text !important;
                }
            `;
            document.head.appendChild(style);
        }
    }

    function addListItem(list) {
        const newItem = document.createElement('li');
        const textSpan = document.createElement('span');
        textSpan.contentEditable = true;
        textSpan.textContent = 'New item';
        newItem.appendChild(textSpan);
        addDeleteButton(newItem);
        list.appendChild(newItem);
    }

    function addDeleteButton(item) {
        const deleteBtn = document.createElement('button');
        deleteBtn.innerHTML = '<i class="fas fa-times"></i>';
        deleteBtn.classList.add('delete-item-btn');
        deleteBtn.addEventListener('click', () => item.remove());
        item.appendChild(deleteBtn);
    }

    async function saveChanges() {
        try {
            const currentUserResponse = await fetch('/api/current-user');
            const currentUserData = await currentUserResponse.json();
            const profileUsername = document.getElementById('profile-username-label').textContent;

            if (!currentUserData.user || currentUserData.user.username !== profileUsername) {
                alert('You are not authorized to edit this profile.');
                location.reload();
                return;
            }

            const profileData = {
                discord_username: document.getElementById('discord-info').textContent,
                steam_id: document.getElementById('steam-info').textContent,
                bio: document.getElementById('bio-text').textContent,
                defcon_username: document.getElementById('defcon-username').textContent,
                years_played: document.getElementById('years-of-service').textContent,
                main_contributions: Array.from(document.getElementById('main-contributions-list').children).map(li => li.textContent.replace('X', '').trim()),
                guides_and_mods: Array.from(document.getElementById('guides-and-mods-list').children).map(li => li.textContent.replace('X', '').trim())
            };

            const response = await fetch('/api/update-profile', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                },
                body: JSON.stringify(profileData),
            });

            const result = await response.json();

            if (result.success) {
                alert('Profile updated successfully!');
            } else {
                alert('Failed to update profile. Please try again.');
            }
        } catch (error) {
            console.error('Error updating profile:', error);
            alert('An error occurred while updating the profile.');
        }

        editableFields.forEach(field => {
            field.contentEditable = false;
            
            field.removeAttribute('data-editing');
        });

        editableLists.forEach(list => {
            const addItemBtn = list.parentNode.querySelector('.add-item-btn');
            if (addItemBtn) addItemBtn.remove();
            list.querySelectorAll('.delete-item-btn').forEach(btn => btn.remove());
        });
    }
}

function formatDurationProfile(minutes) {
    if (!minutes || isNaN(minutes)) return '0 min';

    if (minutes < 60) {
        return `${Math.round(minutes)} min`;
    } else {
        const hours = Math.floor(minutes / 60);
        const remainingMinutes = Math.round(minutes % 60);
        return `${hours} hr ${remainingMinutes} min`;
    }
}

function initializeProfileImageEditing() {
    const profilePictureContainer = document.getElementById('profile-picture-container');
    const profileBannerOverlay = document.getElementById('profile-banner-overlay');
    const profilePictureOverlay = document.getElementById('profile-picture-overlay');
    const imageEditorModal = document.getElementById('image-editor-modal');
    const cropOverlay = document.getElementById('crop-overlay');
    const zoomControl = document.getElementById('zoom-control');
    const skipEditBtn = document.getElementById('skip-edit');
    const cancelEditBtn = document.getElementById('cancel-edit');
    const imageUpload = document.getElementById('image-upload');
    const imageToEdit = document.getElementById('image-to-edit');
    const applyEditBtn = document.getElementById('apply-edit');

    let currentEditingElement = null;
    let isProfilePicture = false;
    let scale = 1;
    let translateX = 0;
    let translateY = 0;
    let imageUrl = ''; 

    if (profileBannerOverlay && profilePictureOverlay) {
        profileBannerOverlay.style.display = 'none';
        profilePictureOverlay.style.display = 'none';
    }

    function toggleEditMode() {
        const isEditing = document.body.classList.contains('editing');
        if (profileBannerOverlay) profileBannerOverlay.style.display = isEditing ? 'flex' : 'none';
        if (profilePictureOverlay) profilePictureOverlay.style.display = isEditing ? 'flex' : 'none';
    }

    function showImageEditor(imageElement, isProfile, url) {
        currentEditingElement = imageElement;
        isProfilePicture = isProfile;
        imageUrl = url || imageElement.src || ''; 
        
        imageToEdit.onload = () => {
            imageEditorModal.style.display = 'block';
            resetZoom();
            updateCropOverlay();
            centerImage();
        };
        
        if (imageUrl) {
            imageToEdit.src = `${imageUrl}?t=${new Date().getTime()}`;
        }
    }

    function centerImage() {
        if (!imageToEdit || !imageToEdit.parentElement) return;
        
        const containerWidth = imageToEdit.parentElement.offsetWidth;
        const containerHeight = imageToEdit.parentElement.offsetHeight;
        const imgWidth = imageToEdit.naturalWidth * scale;
        const imgHeight = imageToEdit.naturalHeight * scale;

        translateX = (containerWidth - imgWidth) / 2;
        translateY = (containerHeight - imgHeight) / 2;

        applyTransform();
    }

    if (imageToEdit) {
        imageToEdit.addEventListener('wheel', (e) => {
            e.preventDefault();
            if (!zoomControl) return;
            
            const delta = e.deltaY > 0 ? -0.1 : 0.1;
            const newZoom = Math.min(Math.max(parseFloat(zoomControl.value) + delta * 100, zoomControl.min), zoomControl.max);
            zoomControl.value = newZoom;
            applyZoom();
        });
    }

    function resetZoom() {
        scale = 1;
        if (zoomControl && zoomControl instanceof HTMLInputElement) {
            zoomControl.value = '100';
        }
        translateX = 0;
        translateY = 0;
        applyTransform();
    }

    function applyZoom() {
        if (!zoomControl || !imageToEdit || !imageToEdit.parentElement) return;
        
        const newScale = parseFloat(zoomControl.value) / 100;
        const deltaScale = newScale - scale;

        const containerWidth = imageToEdit.parentElement.offsetWidth;
        const containerHeight = imageToEdit.parentElement.offsetHeight;
        translateX -= (containerWidth / 2 - translateX) * deltaScale / newScale;
        translateY -= (containerHeight / 2 - translateY) * deltaScale / newScale;

        scale = newScale;
        applyTransform();
    }

    function updateCropOverlay() {
        if (!cropOverlay) return;

        const profilePictureDimensions = 480;
        if (isProfilePicture) {
            cropOverlay.style.borderRadius = '50%';
            cropOverlay.style.width = `${profilePictureDimensions}px`;
            cropOverlay.style.height = `${profilePictureDimensions}px`;
        } else {
            cropOverlay.style.borderRadius = '0';
            cropOverlay.style.width = '100%';
            cropOverlay.style.height = '33.33%';
        }
    }

    if (profilePictureContainer) {
        profilePictureContainer.addEventListener('click', (e) => {
            if (document.body.classList.contains('editing') && e.target.closest('.edit-overlay')) {
                currentEditingElement = document.getElementById('profile-picture');
                isProfilePicture = true;
                if (imageUpload) {
                    imageUpload.click();
                }
            }
        });
    }

    if (imageUpload) {
        imageUpload.addEventListener('change', (e) => {
            const fileInput = e.target;
            if (!(fileInput instanceof HTMLInputElement) || !fileInput.files || fileInput.files.length === 0) return;
            
            const file = fileInput.files[0];
            if (file) {
                const reader = new FileReader();
                reader.onload = (e) => {
                    if (e.target && e.target.result && imageToEdit) {
                        imageToEdit.src = e.target.result.toString();
                        showImageEditor(currentEditingElement, isProfilePicture, e.target.result.toString());
                    }
                };
                reader.readAsDataURL(file);
            }
        });
    }

    if (zoomControl) {
        zoomControl.max = '400';
        zoomControl.addEventListener('input', applyZoom);
    }

    function applyTransform() {
        if (!imageToEdit) return;
        imageToEdit.style.transform = `translate(-50%, -50%) scale(${scale})`;
        imageToEdit.style.left = '50%';
        imageToEdit.style.top = '50%';
    }

    if (skipEditBtn) {
        skipEditBtn.addEventListener('click', () => {
            if (imageEditorModal) imageEditorModal.style.display = 'none';
        });
    }

    if (cancelEditBtn) {
        cancelEditBtn.addEventListener('click', () => {
            if (imageEditorModal) imageEditorModal.style.display = 'none';
        });
    }

    if (applyEditBtn) {
        applyEditBtn.addEventListener('click', () => {
            if (!imageToEdit || !zoomControl) return;
            
            const canvas = document.createElement('canvas');
            const ctx = canvas.getContext('2d');
            if (!ctx) return;
            
            let containerWidth, containerHeight;

            if (isProfilePicture) {
                containerWidth = containerHeight = 480;
            } else {
                if (!imageToEdit.parentElement) return;
                containerWidth = imageToEdit.parentElement.offsetWidth;
                containerHeight = imageToEdit.parentElement.offsetHeight / 3;
            }

            canvas.width = containerWidth;
            canvas.height = containerHeight;

            const scale = parseFloat(zoomControl.value) / 100;
            const imgWidth = imageToEdit.naturalWidth * scale;
            const imgHeight = imageToEdit.naturalHeight * scale;
            const sx = (imgWidth - containerWidth) / 2 / scale;
            const sy = (imgHeight - containerHeight) / 2 / scale;

            ctx.drawImage(
                imageToEdit,
                sx, sy, containerWidth / scale, containerHeight / scale,
                0, 0, containerWidth, containerHeight
            );

            canvas.toBlob(async (blob) => {
                if (!blob) return;
                
                const formData = new FormData();
                formData.append('image', blob, isProfilePicture ? 'profile_image.png' : 'banner_image.png');
                formData.append('type', isProfilePicture ? 'profile' : 'banner');

                try {
                    const response = await fetch('/api/upload-profile-image', {
                        method: 'POST',
                        body: formData
                    });
                    const result = await response.json();
                    if (result.success && currentEditingElement && imageEditorModal) {
                        const newImageUrl = result.imageUrl + '?t=' + new Date().getTime();
                        if (isProfilePicture) {
                            currentEditingElement.src = newImageUrl;
                        } else {
                            currentEditingElement.style.backgroundImage = `url(${newImageUrl})`;
                        }
                        imageEditorModal.style.display = 'none';
                    } else {
                        alert('Failed to upload image. Please try again.');
                    }
                } catch (error) {
                    alert('An error occurred while uploading the image.');
                }
            }, 'image/png');
        });
    }
    
    const observer = new MutationObserver(mutations => {
        mutations.forEach(mutation => {
            if (mutation.attributeName === 'class') {
                toggleEditMode();
            }
        });
    });
    
    observer.observe(document.body, { attributes: true });
    
    toggleEditMode();
}

export { 
    initializeProfileEditing, 
    initializeProfileImageEditing, 
    formatDurationProfile 
};